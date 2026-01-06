#include <Arduino.h>
#include <main.h>
#include <AsyncTCP.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include "wifiFunctions.h"
#include "webFunctions.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "settings.h"
#include "time.h"
#include "rf.h"
#include <RadioLib.h>

//Routing
std::vector<Peer> peerList;
uint32_t announceTimer = 0;


//Uhrzeit 
const char* TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";

//Timing
uint32_t webSocketTimer = millis();
uint32_t rebootTimer = 0xFFFFFFFF;

void sendPeerList() {
  //Abbruch, wenn liste leer
  if (peerList.size() == 0) {return;}
  JsonDocument doc;
  for (int i = 0; i < peerList.size(); i++) {
    Serial.printf("Peer List #%d %s\n", i, peerList[i].call);
    doc["peers"][i]["call"] = peerList[i].call;
    doc["peers"][i]["lastRX"] = peerList[i].lastRX;
    doc["peers"][i]["rssi"] = peerList[i].rssi;
    doc["peers"][i]["snr"] = peerList[i].snr;
    doc["peers"][i]["frqError"] = peerList[i].frqError;
    doc["peers"][i]["available"] = peerList[i].available;
  }  
  String jsonOutput;
  serializeJson(doc, jsonOutput);
  ws.textAll(jsonOutput);

}



void setup() {
  //Protokoll
  peerList.reserve(20);

  //CPU Frqg fest (soll wegen SPI sinnvoll sein)
  setCpuFrequencyMhz(240);

  //SPI Init
  SPI.begin(5, 19, 27, 18);

  //Ausgäne
  pinMode(PIN_WIFI_LED, OUTPUT); //digitalWrite(PIN_WIFI_LED, 0); 

  //Debug-Port
  Serial.begin(115200);
  Serial.setDebugOutput(true);  
  while (!Serial) {}
  Serial.println("--------------------------------------------------------------------------------");

  //Einstellungen laden
  loadSettings();
  defaultSettings();
  if (checkSettings() == false) {
    defaultSettings();
    saveSettings();
  }
  showSettings();

  //WiFi Starten
  wifiInit();

  //Zeit setzzen
  struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
  settimeofday(&tv, NULL);
  configTzTime(TZ_INFO, settings.ntpServer);

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
  } else {
    Serial.println("LittleFS mounted successfully");
  } 

  //WEB-Server starten
  startWebServer();

  //Radio Init
  initRadio();



}

void loop() {
  //WiFi Status anzeigen (LED)
  showWiFiStatus();  

  //Announce Senden
  if (millis() > announceTimer) {
    announceTimer = millis() + ANNOUNCE_TIME + random(0, ANNOUNCE_TIME);
    uint8_t txBuffer[256];
    uint8_t txBufferLength = 1;
    txBuffer[0] = 0x00;  //Frametyp -> ANNOUNCE
    addSourceCall(txBuffer, txBufferLength);
    if (transmit(txBuffer, txBufferLength) == false) {
      //Nochmal in 100mS versuchen
      announceTimer = millis() + random(0, ANNOUNCE_TIME);
    }
  }
  
  if (radioFlag) {
    radioFlag = false;
    uint16_t irqFlags = radio.getIRQFlags();
    //Daten Empfangen
    if (irqFlags == 0x50) {
      //Prüfen, ob was empfangen wurde
      byte rxBytes[256];
      int rxSize = radio.getPacketLength();
      int state = radio.readData(rxBytes, rxSize);
      if (state == RADIOLIB_ERR_NONE) {
        Serial.println("[SX1278] RX");

        //Daten über Websocket senden
        JsonDocument doc;
        for (int i = 0; i < rxSize; i++) {
          doc["monitor"]["data"][i] = rxBytes[i];
        }
        doc["monitor"]["tx"] = false;
        doc["monitor"]["rssi"] = radio.getRSSI();
        doc["monitor"]["snr"] = radio.getSNR();
        doc["monitor"]["frequencyError"] = radio.getFrequencyError();
        String jsonOutput;
        serializeJson(doc, jsonOutput);
        ws.textAll(jsonOutput);

        //Decodieren
        //Rufzeichen ausschneiden
        String srcCall = "";
				String dstCall = "";
        std::vector<String> viaCall;
        //Frame druchlaufen
        for (uint8_t i=1; i < rxSize; i++) {
          //Header prüfen
          switch (rxBytes[i] & 0xF0) {
            case 0x00:
              for(int ii = i + 1; ii < i + 1 + (rxBytes[i] & 0x0F); ii++) { srcCall += (char)rxBytes[ii]; }
              i += rxBytes[i] & 0x0F;
              break;
            case 0x01:
              for(int ii = i + 1; ii < i + 1 + (rxBytes[i] & 0x0F); ii++) { dstCall += (char)rxBytes[ii]; }
              i += rxBytes[i] & 0x0F;
              break;
          }
        }
        //Tryme Typ prüfen
        switch (rxBytes[0]) {
          case 0x00:  //Announce 
            Serial.printf("SRC: %s\n", srcCall);
            Serial.printf("DST: %s\n", dstCall);

            //In Peer-Liste einfügen
            Peer p;
            p.call = srcCall;
            p.lastRX = time(NULL);
            p.frqError = radio.getFrequencyError();
            p.rssi = radio.getRSSI();
            p.snr = radio.getSNR();
            p.available = false;
            peerList.push_back(p);
            sendPeerList();

            
            //Antworten -> MUSS SPÄTER IN SENDE_PUFFER MIT DEFINIERTER ZEIT, WENN GESENDET WIRD !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            uint8_t txBufferLength = 0;
            uint8_t txBuffer[256];
            //Frametyp -> ANNOUNCE REPLY
            txBufferLength += 1; txBuffer[0] = 0x01;  
            //Absender hinzu
            addSourceCall(txBuffer, txBufferLength);
            //Empfänger hinzu
            txBuffer[txBufferLength] = 0x10 | (0x0F & srcCall.length());  //Header Empfänger
            txBufferLength += 1; 
            memcpy(&txBuffer[txBufferLength], &srcCall[0], srcCall.length()); //Empfänger Payload
            txBufferLength += srcCall.length();
            //Senden
            transmit(txBuffer, txBufferLength);

            break;

        }




      }  
      radio.startReceive();
    }
    //Senden fertig
    if (irqFlags == 0x08) {
      radio.startReceive();
    }
  }

  //Status über Websocket senden
  if (millis() > webSocketTimer) {
    webSocketTimer = millis() + 1000;
    JsonDocument doc;
    doc["time"] = time(NULL);
    String jsonOutput;
    serializeJson(doc, jsonOutput);
    ws.textAll(jsonOutput);





    // Serial.println(time(NULL));
    // struct tm timeinfo;
    // time_t now;
    // time(&now);
    // localtime_r(&now, &timeinfo);
    // char timeString[20]; 
    // strftime(timeString, sizeof(timeString), "%d.%m.%Y %H:%M:%S", &timeinfo); // %d=Tag, %m=Monat, %Y=Jahr, %H=Stunde, %M=Minute, %S=Sekunde
    // Serial.println(timeString);

  }

  //Reboot
  if (millis() > rebootTimer) {ESP.restart();}
}