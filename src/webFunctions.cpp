//#define WS_MAX_QUEUED_MESSAGES 1

#include "webFunctions.h"
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include "settings.h"
#include "wifiFunctions.h"
#include <WiFi.h>
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "main.h"
#include "esp_task_wdt.h"
#include "rf.h"


AsyncWebServer webServer(80);
// create an easy-to-use handler
AsyncWebSocketMessageHandler wsHandler;

// add it to the websocket server
AsyncWebSocket ws("/socket", wsHandler.eventHandler());

// https://github.com/ESP32Async/ESPAsyncWebServer/wiki


void startWebServer() {

  //---------------------- WEBSOCKET -------------------------

  wsHandler.onConnect([](AsyncWebSocket *server, AsyncWebSocketClient *client) {
    Serial.printf("Client %" PRIu32 " connected\n", client->id());
    sendSettings();
    sendPeerList();
    ws.cleanupClients();
  });

  wsHandler.onDisconnect([](AsyncWebSocket *server, uint32_t clientId) {
    Serial.printf("Client %" PRIu32 " disconnected\n", clientId);
    ws.cleanupClients();
  });

  wsHandler.onError([](AsyncWebSocket *server, AsyncWebSocketClient *client, uint16_t errorCode, const char *reason, size_t len) {
    Serial.printf("Client %" PRIu32 " error: %" PRIu16 ": %s\n", client->id(), errorCode, reason);
    ws.cleanupClients();
  });

  //Empfangene Daten auswerten
  wsHandler.onMessage([](AsyncWebSocket *server, AsyncWebSocketClient *client, const uint8_t *data, size_t len) {
    //JSON 
    JsonDocument json;
    DeserializationError error = deserializeJson(json, data, len);

    //PING (nur zum test)
    if (json["ping"].is<JsonVariant>()) {
      //Serial.println(json["ping"].as<String>());
    }

    //Einstellungen speichern
    if (json["settings"].is<JsonVariant>()) {
      Serial.println("Einstellungen");

      if (json["settings"]["mycall"].is<JsonVariant>()) { 
        String mycall = json["settings"]["mycall"].as<String>();
        mycall.toUpperCase();
        mycall.toCharArray(settings.mycall, sizeof(settings.mycall));
        //strlcpy(settings.mycall, json["settings"]["mycall"] | "", sizeof(settings.mycall)); 
      }
      if (json["settings"]["ntp"].is<JsonVariant>()) { strlcpy(settings.ntpServer, json["settings"]["ntp"] | "", sizeof(settings.ntpServer)); }
      if (json["settings"]["dhcpActive"].is<JsonVariant>()) { settings.dhcpActive = json["settings"]["dhcpActive"].as<bool>(); }
      if (json["settings"]["wifiSSID"].is<JsonVariant>()) { strlcpy(settings.wifiSSID, json["settings"]["wifiSSID"] | "", sizeof(settings.wifiSSID)); }
      if (json["settings"]["wifiPassword"].is<JsonVariant>()) { strlcpy(settings.wifiPassword, json["settings"]["wifiPassword"] | "", sizeof(settings.wifiPassword)); }
      if (json["settings"]["apMode"].is<JsonVariant>()) { settings.apMode = json["settings"]["apMode"].as<bool>(); }
      if (json["settings"]["wifiIP"].is<JsonVariant>()) { 
        JsonArray ipArray = json["settings"]["wifiIP"];
        for (int i = 0; i < 4; i++) {settings.wifiIP[i] = ipArray[i] | 0; }
      }
      if (json["settings"]["wifiNetMask"].is<JsonVariant>()) { 
        JsonArray ipArray = json["settings"]["wifiNetMask"];
        for (int i = 0; i < 4; i++) {settings.wifiNetMask[i] = ipArray[i] | 0; }
      }
      if (json["settings"]["wifiGateway"].is<JsonVariant>()) { 
        JsonArray ipArray = json["settings"]["wifiGateway"];
        for (int i = 0; i < 4; i++) {settings.wifiGateway[i] = ipArray[i] | 0; }
      }
      if (json["settings"]["wifiDNS"].is<JsonVariant>()) { 
        JsonArray ipArray = json["settings"]["wifiDNS"];
        for (int i = 0; i < 4; i++) {settings.wifiDNS[i] = ipArray[i] | 0; }
      }
      if (json["settings"]["loraFrequency"].is<JsonVariant>()) { settings.loraFrequency = json["settings"]["loraFrequency"].as<float>(); }
      if (json["settings"]["loraOutputPower"].is<JsonVariant>()) { settings.loraOutputPower = json["settings"]["loraOutputPower"].as<int8_t>(); }
      if (json["settings"]["loraBandwidth"].is<JsonVariant>()) { settings.loraBandwidth = json["settings"]["loraBandwidth"].as<float>(); }
      if (json["settings"]["loraSyncWord"].is<JsonVariant>()) { settings.loraSyncWord = json["settings"]["loraSyncWord"].as<uint8_t>(); }
      if (json["settings"]["loraCodingRate"].is<JsonVariant>()) { settings.loraCodingRate = json["settings"]["loraCodingRate"].as<uint8_t>(); }
      if (json["settings"]["loraSpreadingFactor"].is<JsonVariant>()) { settings.loraSpreadingFactor = json["settings"]["loraSpreadingFactor"].as<uint8_t>(); }
      if (json["settings"]["loraPreambleLength"].is<JsonVariant>()) { settings.loraPreambleLength = json["settings"]["loraPreambleLength"].as<int16_t>(); }
      initRadio();
      saveSettings();
    }

      

    //Uhrzeit Sync
    if (json["time"].is<JsonVariant>()) {
      struct timeval tv;
      tv.tv_sec = json["time"].as<time_t>();
      tv.tv_usec = 0;
      settimeofday(&tv, NULL);
    }

    //WiFi Scannen
    if (json["scanWifi"].is<JsonVariant>()) {
      Serial.println("WiFi Scan....");
      WiFi.scanNetworks(true);
    }

    //Announce
    if (json["announce"].is<JsonVariant>()) {
      Serial.println("Send manual announce....");
      announceTimer = 0;
    }  

    //Tune
    if (json["tune"].is<JsonVariant>()) {
      Serial.println("Send tune...");
      Frame f;
      f.frameType = FrameType::TUNE;
      f.transmitMillis = 0;
      //Frame in SendeBuffer
      txFrameBuffer.push_back(f);
    }     

    //Reboot
    if (json["reboot"].is<JsonVariant>()) {
      Serial.println("Reboot");
      rebootTimer = millis() + 2500;
    }    


  });
  
  //Websocket -> Webserver
  webServer.addHandler(&ws);

  //---------------------- WEBSERVER -------------------------

  //Redirect für Index-Seite
   webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("/index.html");
  }); 

  // //Dynamische Inhalte INDEX.HTML
  // webServer.on("/index.html", HTTP_ANY, [](AsyncWebServerRequest *request){
  //   request->send(LittleFS, "/index.html", String(), false, webProcessor);
  // });

  // //Dynamische Inhalte REBOOT.HTML
  // webServer.on("/reboot.html", HTTP_ANY, [](AsyncWebServerRequest *request){
  //   if (request->hasParam("REBOOT", true)) { 
  //     Serial.println("Reboot");
  //     //rebootTimer = millis() + 2500;
  //   }
  //   request->send(LittleFS, "/reboot.html", String(), false, webProcessor);
  // });

  // //Dynamische Inhalte SETTINGS.HTML 
  // webServer.on("/settings.html", HTTP_ANY, [](AsyncWebServerRequest *request){
  //   //Netzwerke scannen
  //   if (request->hasParam("SEARCH", true)) {
  //     //RTOS Watchdog aus
  //     Serial.println("Scanne WiFi");
  //     esp_task_wdt_deinit();
  //     WiFi.scanNetworks();
  //     request->redirect("/settings.html");
  //     return;
  //   } 
  //   if (request->hasParam("NAME", true)) { request->getParam("NAME", true)->value().toCharArray(settings.name, 64); }
  //   if (request->hasParam("WIFI_SSID", true)) { request->getParam("WIFI_SSID", true)->value().toCharArray(settings.wifiSSID, 64); }
  //   if (request->hasParam("WIFI_PASSWORD", true)) { request->getParam("WIFI_PASSWORD", true)->value().toCharArray(settings.wifiPassword, 64); }
  //   if (request->method() == HTTP_POST ) { if (request->hasParam("AP_MODE", true)) {settings.apMode = true;} else {settings.apMode = false;} }
  //   if (request->method() == HTTP_POST ) { if (request->hasParam("DHCP", true)) {settings.dhcpActive = true;} else {settings.dhcpActive = false;} }
  //   if (request->hasParam("WIFI_IP_0", true)) {settings.wifiIP[0] = request->getParam("WIFI_IP_0", true)->value().toInt();}
  //   if (request->hasParam("WIFI_IP_1", true)) {settings.wifiIP[1] = request->getParam("WIFI_IP_1", true)->value().toInt();}
  //   if (request->hasParam("WIFI_IP_2", true)) {settings.wifiIP[2] = request->getParam("WIFI_IP_2", true)->value().toInt();}
  //   if (request->hasParam("WIFI_IP_3", true)) {settings.wifiIP[3] = request->getParam("WIFI_IP_3", true)->value().toInt();}
  //   if (request->hasParam("WIFI_NETMASK_0", true)) {settings.wifiNetMask[0] = request->getParam("WIFI_NETMASK_0", true)->value().toInt();}
  //   if (request->hasParam("WIFI_NETMASK_1", true)) {settings.wifiNetMask[1] = request->getParam("WIFI_NETMASK_1", true)->value().toInt();}
  //   if (request->hasParam("WIFI_NETMASK_2", true)) {settings.wifiNetMask[2] = request->getParam("WIFI_NETMASK_2", true)->value().toInt();}
  //   if (request->hasParam("WIFI_NETMASK_3", true)) {settings.wifiNetMask[3] = request->getParam("WIFI_NETMASK_3", true)->value().toInt();}
  //   if (request->hasParam("WIFI_GATEWAY_0", true)) {settings.wifiGateway[0] = request->getParam("WIFI_GATEWAY_0", true)->value().toInt();}
  //   if (request->hasParam("WIFI_GATEWAY_1", true)) {settings.wifiGateway[1] = request->getParam("WIFI_GATEWAY_1", true)->value().toInt();}
  //   if (request->hasParam("WIFI_GATEWAY_2", true)) {settings.wifiGateway[2] = request->getParam("WIFI_GATEWAY_2", true)->value().toInt();}
  //   if (request->hasParam("WIFI_GATEWAY_3", true)) {settings.wifiGateway[3] = request->getParam("WIFI_GATEWAY_3", true)->value().toInt();}
  //   if (request->hasParam("WIFI_DNS_0", true)) {settings.wifiDNS[0] = request->getParam("WIFI_DNS_0", true)->value().toInt();}
  //   if (request->hasParam("WIFI_DNS_1", true)) {settings.wifiDNS[1] = request->getParam("WIFI_DNS_1", true)->value().toInt();}
  //   if (request->hasParam("WIFI_DNS_2", true)) {settings.wifiDNS[2] = request->getParam("WIFI_DNS_2", true)->value().toInt();}
  //   if (request->hasParam("WIFI_DNS_3", true)) {settings.wifiDNS[3] = request->getParam("WIFI_DNS_3", true)->value().toInt();}
  //   if (request->hasParam("SAVE", true)) {
  //     //Einstellungen speichern + Redirect auf neue IP
  //     showSettings();
  //     saveSettings();
  //     request->redirect("/reboot.html");
  //     return;
  //   }
  //   request->send(LittleFS, "/settings.html", String(), false, webProcessor);
  // });

  //Statische Webseite aus Filesystem  + Starten
  webServer.serveStatic("/", LittleFS, "/");
  webServer.begin(); 
}


// String webProcessor(const String& var){
//   if (var == "NAME") { return settings.name; }
//   if (var == "AP_MODE") { if (settings.apMode) {return "checked";} }
//   if (var == "DHCP") { if (settings.dhcpActive) {return "checked";} }
//   if (var == "WIFI_PASSWORD") { return settings.wifiPassword; }
//   if (settings.dhcpActive) {
//     if (var == "WIFI_IP_0") { return String(WiFi.localIP()[0]); }
//     if (var == "WIFI_IP_1") { return String(WiFi.localIP()[1]); }
//     if (var == "WIFI_IP_2") { return String(WiFi.localIP()[2]); }
//     if (var == "WIFI_IP_3") { return String(WiFi.localIP()[3]); }
//     if (var == "WIFI_NETMASK_0") { return String(WiFi.subnetMask()[0]); }
//     if (var == "WIFI_NETMASK_1") { return String(WiFi.subnetMask()[1]); }
//     if (var == "WIFI_NETMASK_2") { return String(WiFi.subnetMask()[2]); }
//     if (var == "WIFI_NETMASK_3") { return String(WiFi.subnetMask()[3]); }
//     if (var == "WIFI_GATEWAY_0") { return String(WiFi.gatewayIP()[0]); }
//     if (var == "WIFI_GATEWAY_1") { return String(WiFi.gatewayIP()[1]); }
//     if (var == "WIFI_GATEWAY_2") { return String(WiFi.gatewayIP()[2]); }
//     if (var == "WIFI_GATEWAY_3") { return String(WiFi.gatewayIP()[3]); }
//     if (var == "WIFI_DNS_0") { return String(WiFi.dnsIP()[0]); }
//     if (var == "WIFI_DNS_1") { return String(WiFi.dnsIP()[1]); }
//     if (var == "WIFI_DNS_2") { return String(WiFi.dnsIP()[2]); }
//     if (var == "WIFI_DNS_3") { return String(WiFi.dnsIP()[3]); }
//   } else {
//     if (var == "WIFI_IP_0") { return String(settings.wifiIP[0]); }
//     if (var == "WIFI_IP_1") { return String(settings.wifiIP[1]); }
//     if (var == "WIFI_IP_2") { return String(settings.wifiIP[2]); }
//     if (var == "WIFI_IP_3") { return String(settings.wifiIP[3]); }
//     if (var == "WIFI_NETMASK_0") { return String(settings.wifiNetMask[0]); }
//     if (var == "WIFI_NETMASK_1") { return String(settings.wifiNetMask[1]); }
//     if (var == "WIFI_NETMASK_2") { return String(settings.wifiNetMask[2]); }
//     if (var == "WIFI_NETMASK_3") { return String(settings.wifiNetMask[3]); }
//     if (var == "WIFI_GATEWAY_0") { return String(settings.wifiGateway[0]); }
//     if (var == "WIFI_GATEWAY_1") { return String(settings.wifiGateway[1]); }
//     if (var == "WIFI_GATEWAY_2") { return String(settings.wifiGateway[2]); }
//     if (var == "WIFI_GATEWAY_3") { return String(settings.wifiGateway[3]); }
//     if (var == "WIFI_DNS_0") { return String(settings.wifiDNS[0]); }
//     if (var == "WIFI_DNS_1") { return String(settings.wifiDNS[1]); }
//     if (var == "WIFI_DNS_2") { return String(settings.wifiDNS[2]); }
//     if (var == "WIFI_DNS_3") { return String(settings.wifiDNS[3]); }
//   }
//   if (var == "SSID_OPTIONS") {
//     //WiFi-Scan Result
//     uint16_t networkCount = 0;
//     String ssidOptions = "";
//     bool ssidOK = false;
//     do {
//       if (WiFi.SSID(networkCount).length() > 0) {
//         ssidOptions += "<option value='" + WiFi.SSID(networkCount) + "'";
//         if (WiFi.SSID(networkCount) == settings.wifiSSID) {
//           ssidOptions += " selected"; 
//           ssidOK = true; 
//         }
//         ssidOptions += ">" + WiFi.SSID(networkCount) + "</option> \r\n";      
//       }
//       networkCount++;
//     }
//     while (WiFi.SSID(networkCount).length() > 0 && networkCount < 32);
//     if (!ssidOK) {
//       ssidOptions += "<option value='" + String(settings.wifiSSID) + "' selected>" + String(settings.wifiSSID) + "</option> \r\n";
//     }
//     return ssidOptions;
//   }
//   if (var == "URL") {
//       //Neue URL definieren
//       if (settings.apMode == true) {
//         return "http://192.168.1.1";
//       } else {
//         if (settings.dhcpActive == true) {
//           return "http://" + String(WiFi.localIP()[0]) + "." + String(WiFi.localIP()[1]) + "." + String(WiFi.localIP()[2]) + "." + String(WiFi.localIP()[3]);
//         } else {
//           return "http://" + String(settings.wifiIP[0]) + "." + String(settings.wifiIP[1]) + "." + String(settings.wifiIP[2]) + "." + String(settings.wifiIP[3]);
//         }
//       }
//   }
//   return String();
// }



