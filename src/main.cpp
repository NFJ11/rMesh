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
uint32_t announceTimer = 5000; //Erstes Announce nach 5 Sekunden

//Uhrzeit 
const char* TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";

//Timing
uint32_t webSocketTimer = millis();
uint32_t rebootTimer = 0xFFFFFFFF;





void setup() {


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
    announceTimer = millis() + ANNOUNCE_TIME;
    Frame f;
    f.frameType = FrameType::ANNOUNCE;
    f.transmitMillis = 0;
    //Frame in SendeBuffer
    txFrameBuffer.push_back(f);
  }
  
  //Prüfen, ob was gesendet werden muss
  if (transmittingFlag == false) {
    for (int i = 0; i < txFrameBuffer.size(); i++) {
      if (millis() > txFrameBuffer[i].transmitMillis) {
        //Frame senden
        if (transmitFrame(txFrameBuffer[i])) {
          //Senden OK
          //Aus Liste löschen
          txFrameBuffer.erase(txFrameBuffer.begin() + i);
        }else {
          //Nochmal versuchen
          txFrameBuffer[i].transmitMillis = millis() + TX_BUSY_RETRY;
        }
      }
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
        //Daten über Websocket senden
        JsonDocument doc;
        for (int i = 0; i < rxSize; i++) {
          doc["monitor"]["data"][i] = rxBytes[i];
        }
        doc["monitor"]["tx"] = false;
        doc["monitor"]["rssi"] = radio.getRSSI();
        doc["monitor"]["snr"] = radio.getSNR();
        doc["monitor"]["frequencyError"] = radio.getFrequencyError();
        doc["monitor"]["time"] = time(NULL);
        String jsonOutput;
        serializeJson(doc, jsonOutput);
        ws.textAll(jsonOutput);

        //Decodieren
        Frame rxFrame;
        rxFrame.viaCall.reserve(6);
        rxFrame.viaHeader.reserve(6);
        //Frametype
        rxFrame.frameType = rxBytes[0];
        //Frame druchlaufen und nach Headern suchen
        for (uint8_t i=1; i < rxSize; i++) {
          //Header prüfen
          switch (rxBytes[i] & 0xF0) {
            case HeaderType::SRC_CALL:
              for(int ii = i + 1; ii < i + 1 + (rxBytes[i] & 0x0F); ii++) { rxFrame.srcCall += (char)rxBytes[ii]; }
              i += (rxBytes[i] & 0x0F);
              break;
            case HeaderType::DST_CALL:
              for(int ii = i + 1; ii < i + 1 + (rxBytes[i] & 0x0F); ii++) { rxFrame.dstCall += (char)rxBytes[ii]; }
              i += (rxBytes[i] & 0x0F);
              break;
          }
        }

        //In Peer-Liste einfügen
        Peer p;
        p.call = rxFrame.srcCall;
        p.lastRX = time(NULL);
        p.frqError = radio.getFrequencyError();
        p.rssi = radio.getRSSI();
        p.snr = radio.getSNR();
        if ((rxFrame.dstCall == String(settings.mycall)) && (rxFrame.frameType == FrameType::ANNOUNCE_REPLY))  {
          p.available = true;
        } else {
          p.available = false;
        }
        addPeerList(p);

        //Frame Typ prüfen & Antwort basteln
        Frame txFrame;
        switch (rxFrame.frameType) {
          case FrameType::ANNOUNCE:  //Announce 
            //Antowrt zusammenbauen
            txFrame.frameType = FrameType::ANNOUNCE_REPLY;
            txFrame.transmitMillis = millis() + ANNOUNCE_REPLAY_TIME;
            txFrame.dstCall = rxFrame.srcCall;
            txFrameBuffer.push_back(txFrame);
            break;          
          case FrameType::ANNOUNCE_REPLY:  //Announce REPLAY
            //Wird oben bei der Peer-Liste abgehandelt
            break;
        }
      }  
      radio.startReceive();
    }
    //Senden fertig
    if (irqFlags == 0x08) {
      transmittingFlag = false;
      radio.startReceive();
    }
  }

  //Status über Websocket senden
  if (millis() > webSocketTimer) {
    webSocketTimer = millis() + 1000;

    //Zeit über Websocket senden
    JsonDocument doc;
    doc["time"] = time(NULL);
    String jsonOutput;
    serializeJson(doc, jsonOutput);
    ws.textAll(jsonOutput);

    //Peer-Liste checken
    checkPeerList();

  }

  //Reboot
  if (millis() > rebootTimer) {ESP.restart();}
}