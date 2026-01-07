#include <Arduino.h>
#include <EEPROM.h>
#include <nvs_flash.h>
#include "settings.h"
#include <ArduinoJson.h>
#include "wifiFunctions.h"
#include "main.h"


Settings settings;

void defaultSettings() {
    //Standart Einstellungen
    Serial.println("Lade Default-Settings");
    String("MOD2").toCharArray(settings.mycall, 8);
    String("NFJ-Lan BB").toCharArray(settings.wifiSSID, 64);
    String("438.725db0kch").toCharArray(settings.wifiPassword, 64);
    String("de.pool.ntp.org").toCharArray(settings.ntpServer, 64);
    settings.apMode = false;
    settings.dhcpActive = false;
    settings.wifiIP = IPAddress(192,168,33,60);
    settings.wifiNetMask = IPAddress(255,255,255,0);
    settings.wifiGateway = IPAddress(192,168,33,4);
    settings.wifiDNS = IPAddress(192,168,33,4);
    settings.loraFrequency = 434.9375;
    settings.loraOutputPower = 20;
    settings.loraBandwidth = 250;
    settings.loraSyncWord = 0x2b;
    settings.loraCodingRate = 6;
    settings.loraSpreadingFactor = 11;
    settings.loraPreambleLength = 8;
    saveSettings();
}

void saveSettings() {
    //Einstellungen im EEPROM speichern
    Serial.println("Speichere Einstellungen");
    nvs_flash_erase(); // Löscht die gesamte NVS-Partition
    nvs_flash_init();  // Initialisiert sie neu    
    EEPROM.begin(4096);
    EEPROM.put(0, settings);
    EEPROM.commit();
    sendSettings();
    showSettings();
}


void loadSettings() {
  //Einstellungen im EEPROM speichern
  Serial.println("Lade Einstellungen");
  EEPROM.begin(4095);
  EEPROM.get(0, settings); 
}

bool checkSettings() {
    //Prüfen, ob Daten im EEPROM gültig sind
    Serial.println("Prüfe Einstellungen");
    bool valid = true;
    if (String(settings.wifiSSID).length() == 0 || String(settings.wifiSSID).length() >= 64) {valid = false;}
    if (String(settings.wifiPassword).length() == 0 || String(settings.wifiPassword).length() >= 64) {valid = false;}
    return valid;
}

void showSettings() {
    //Einstellungen als Debug-Ausgabe
    Serial.println();
    Serial.println("Einstellungen:");
    Serial.printf("WiFi SSID: %s\n", settings.wifiSSID);
    Serial.printf("WiFi Password: %s\n", settings.wifiPassword);
    Serial.printf("AP-Mode: %s\n", settings.apMode ? "true" : "false");
    Serial.printf("DHCP: %s\n", settings.dhcpActive ? "true" : "false");
    if (!settings.dhcpActive) {
        Serial.printf("IP: %d.%d.%d.%d\n", settings.wifiIP[0], settings.wifiIP[1], settings.wifiIP[2], settings.wifiIP[3]);
        Serial.printf("NetMask: %d.%d.%d.%d\n", settings.wifiNetMask[0], settings.wifiNetMask[1], settings.wifiNetMask[2], settings.wifiNetMask[3]);
        Serial.printf("DNS: %d.%d.%d.%d\n", settings.wifiDNS[0], settings.wifiDNS[1], settings.wifiDNS[2], settings.wifiDNS[3]);
        Serial.printf("Gateway: %d.%d.%d.%d\n", settings.wifiGateway[0], settings.wifiGateway[1], settings.wifiGateway[2], settings.wifiGateway[3]);
    }
    Serial.printf("NTP-Server: %s\n", settings.ntpServer);
    Serial.printf("myCall: %s\n", settings.mycall);
    Serial.printf("loraFrequency: %f\n", settings.loraFrequency);
    Serial.printf("loraOutputPower: %d\n", settings.loraOutputPower);
    Serial.printf("loraBandwidth: %f\n", settings.loraBandwidth);
    Serial.printf("loraSyncWord: %X\n", settings.loraSyncWord);
    Serial.printf("loraCodingRate: %d\n", settings.loraCodingRate);
    Serial.printf("loraSpreadingFactor: %d\n", settings.loraSpreadingFactor);
    Serial.printf("loraPreambleLength: %d\n", settings.loraPreambleLength);
    Serial.println();
}

void sendSettings() {
    //Einstellungen über Websocket senden
    JsonDocument doc;
    doc["settings"]["mycall"] = settings.mycall;
    doc["settings"]["ntp"] = settings.ntpServer;
    doc["settings"]["dhcpActive"] = settings.dhcpActive;
    doc["settings"]["wifiSSID"] = settings.wifiSSID;
    doc["settings"]["wifiPassword"] = settings.wifiPassword;
    doc["settings"]["apMode"] = settings.apMode;
    doc["settings"]["wifiIP"][0] = settings.wifiIP[0];
    doc["settings"]["wifiIP"][1] = settings.wifiIP[1];
    doc["settings"]["wifiIP"][2] = settings.wifiIP[2];
    doc["settings"]["wifiIP"][3] = settings.wifiIP[3];
    doc["settings"]["wifiNetMask"][0] = settings.wifiNetMask[0];
    doc["settings"]["wifiNetMask"][1] = settings.wifiNetMask[1];
    doc["settings"]["wifiNetMask"][2] = settings.wifiNetMask[2];
    doc["settings"]["wifiNetMask"][3] = settings.wifiNetMask[3];
    doc["settings"]["wifiGateway"][0] = settings.wifiGateway[0];
    doc["settings"]["wifiGateway"][1] = settings.wifiGateway[1];
    doc["settings"]["wifiGateway"][2] = settings.wifiGateway[2];
    doc["settings"]["wifiGateway"][3] = settings.wifiGateway[3];
    doc["settings"]["wifiDNS"][0] = settings.wifiDNS[0];
    doc["settings"]["wifiDNS"][1] = settings.wifiDNS[1];
    doc["settings"]["wifiDNS"][2] = settings.wifiDNS[2];
    doc["settings"]["wifiDNS"][3] = settings.wifiDNS[3];
    doc["settings"]["loraFrequency"] = settings.loraFrequency;
    doc["settings"]["loraOutputPower"] = settings.loraOutputPower;
    doc["settings"]["loraBandwidth"] = settings.loraBandwidth;
    doc["settings"]["loraSyncWord"] = settings.loraSyncWord;
    doc["settings"]["loraCodingRate"] = settings.loraCodingRate;
    doc["settings"]["loraSpreadingFactor"] = settings.loraSpreadingFactor;
    doc["settings"]["loraPreambleLength"] = settings.loraPreambleLength;
    doc["settings"]["version"] = VERSION;
    doc["settings"]["name"] = NAME;


  String jsonOutput;
  serializeJson(doc, jsonOutput);
  ws.textAll(jsonOutput);

}