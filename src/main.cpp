#include <Arduino.h>
#include <RadioLib.h>
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

//Timing
uint32_t webSocketTimer = millis();
uint32_t rebootTimer = 0xFFFFFFFF;


//RadioLib config
SX1278 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, LORA_DIO1);
bool receivedFlag = false;
#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif

void setRxFlag(void) {
  // we got a packet, set the flag
  receivedFlag = true;
}

bool transmittedFlag = false;
void setTxFlag(void) {
  // we sent a packet, set the flag
  transmittedFlag = true;
}

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
  if (checkSettings() == false) {
    defaultSettings();
    saveSettings();
  }
  showSettings();

  //WiFi Starten
  wifiInit();
  WiFi.setSleep(false);
  WiFi.setHostname(settings.name);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);  

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
  } else {
    Serial.println("LittleFS mounted successfully");
  } 

  //WEB-Server starten
  startWebServer();
  
}

void loop() {
  //WiFi Status anzeigen (LED)
  showWiFiStatus();  

  //Reboot
  if (millis() > rebootTimer) {ESP.restart();}
}