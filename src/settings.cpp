#include <Arduino.h>
#include <EEPROM.h>
#include <nvs_flash.h>
#include "settings.h"

Settings settings;

void defaultSettings() {
    //Standart Einstellungen
    Serial.println("Lade Default-Settings");
    String("ULE").toCharArray(settings.name, 64);
    String("NFJ-Lan BB").toCharArray(settings.wifiSSID, 64);
    String("438.725db0kch").toCharArray(settings.wifiPassword, 64);
    settings.apMode = false;
    settings.dhcpActive = false;
    settings.wifiIP = IPAddress(192,168,33,80);
    settings.wifiNetMask = IPAddress(255,255,255,0);
    settings.wifiGateway = IPAddress(192,168,33,4);
    settings.wifiDNS = IPAddress(192,168,33,4);
}

void saveSettings() {
    //Einstellungen im EEPROM speichern
    Serial.println("Speichere Einstellungen");
    nvs_flash_erase(); // Löscht die gesamte NVS-Partition
    nvs_flash_init();  // Initialisiert sie neu    
    EEPROM.begin(4096);
    EEPROM.put(0, settings);
    EEPROM.commit();
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
    if (String(settings.name).length() == 0 || String(settings.name).length() >= 64) {valid = false;}
    if (String(settings.wifiSSID).length() == 0 || String(settings.name).length() >= 64) {valid = false;}
    if (String(settings.wifiPassword).length() == 0 || String(settings.name).length() >= 64) {valid = false;}
    return valid;
}

void showSettings() {
    //Einstellungen als Debug-Ausgabe
    Serial.println();
    Serial.println("Einstellungen:");
    Serial.printf("Modul-Name: %s\n", settings.name);
    Serial.printf("WiFi SSID: %s\n", settings.wifiSSID);
    //Serial.printf("WiFi Password: %s\n", settings.wifiPassword);
    Serial.printf("AP-Mode: %s\n", settings.apMode ? "true" : "false");
    Serial.printf("DHCP: %s\n", settings.dhcpActive ? "true" : "false");
    if (!settings.dhcpActive) {
        Serial.printf("IP: %d.%d.%d.%d\n", settings.wifiIP[0], settings.wifiIP[1], settings.wifiIP[2], settings.wifiIP[3]);
        Serial.printf("NetMask: %d.%d.%d.%d\n", settings.wifiNetMask[0], settings.wifiNetMask[1], settings.wifiNetMask[2], settings.wifiNetMask[3]);
        Serial.printf("DNS: %d.%d.%d.%d\n", settings.wifiDNS[0], settings.wifiDNS[1], settings.wifiDNS[2], settings.wifiDNS[3]);
        Serial.printf("Gateway: %d.%d.%d.%d\n", settings.wifiGateway[0], settings.wifiGateway[1], settings.wifiGateway[2], settings.wifiGateway[3]);
    }
    Serial.println();
}