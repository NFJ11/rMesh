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
//Peer peerList[20];
uint32_t announceTimer = 0;


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
    announceTimer = millis() + ANNOUNCE_TIME + random(0, ANNOUNCE_TIME);
    size_t mycallLen = strlen(settings.mycall); 
    uint8_t txBuffer[256];
    txBuffer[0] = 0x00;  //Frametyp -> ANNOUNCE
    txBuffer[1] = 0x00 | (0x0F & mycallLen);  //Header Absender
    memcpy(&txBuffer[2], &settings.mycall[0], mycallLen); //Payload
    if (transmit(txBuffer, mycallLen + 2) == false) {
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