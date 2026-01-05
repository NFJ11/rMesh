#include <Arduino.h>
#include <WiFi.h>
#include "wifiFunctions.h"
#include "settings.h"
#include "main.h"

unsigned long long ledTimer = 0;
byte wifiStatus = 0xff;

void wifiInit() {
  //Wifi Init
  WiFi.mode(WIFI_STA);
  if (settings.apMode) {
    //AP-Mode
    Serial.println("Starte WiFi AP-Mode");
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192,168,1,1), IPAddress(192,168,1,1), IPAddress(255,255,255,0));
    WiFi.softAP(String(settings.name));
  } else {
    Serial.println("Starte WiFi Client-Mode");
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    if (!settings.dhcpActive) {
        //Feste IP
        WiFi.config(settings.wifiIP, settings.wifiGateway, settings.wifiNetMask, settings.wifiDNS);
    }
    WiFi.begin(settings.wifiSSID, settings.wifiPassword);
  }
}

void debugAvailableNetworks() {
  int n = WiFi.scanNetworks();

  if (n == 0) {
        Serial.println("no networks found");
    } else {
        Serial.print(n);
        Serial.println(" networks found");
        Serial.println("Nr | SSID                             | RSSI | CH | Encryption");
        for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            Serial.printf("%2d",i + 1);
            Serial.print(" | ");
            Serial.printf("%-32.32s", WiFi.SSID(i).c_str());
            Serial.print(" | ");
            Serial.printf("%4d", WiFi.RSSI(i));
            Serial.print(" | ");
            Serial.printf("%2d", WiFi.channel(i));
            Serial.print(" | ");
            switch (WiFi.encryptionType(i))
            {
            case WIFI_AUTH_OPEN:
                Serial.print("open");
                break;
            case WIFI_AUTH_WEP:
                Serial.print("WEP");
                break;
            case WIFI_AUTH_WPA_PSK:
                Serial.print("WPA");
                break;
            case WIFI_AUTH_WPA2_PSK:
                Serial.print("WPA2");
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                Serial.print("WPA+WPA2");
                break;
            case WIFI_AUTH_WPA2_ENTERPRISE:
                Serial.print("WPA2-EAP");
                break;
            case WIFI_AUTH_WPA3_PSK:
                Serial.print("WPA3");
                break;
            case WIFI_AUTH_WPA2_WPA3_PSK:
                Serial.print("WPA2+WPA3");
                break;
            case WIFI_AUTH_WAPI_PSK:
                Serial.print("WAPI");
                break;
            default:
                Serial.print("unknown");
            }
            Serial.println();
            delay(10);
        }
    }
    Serial.println("");
}

void showWiFiStatus() {
  //Status-LED
  if (settings.apMode) {
    //AP-Mode
    if (millis() > ledTimer) {
      digitalWrite(PIN_WIFI_LED, !digitalRead(PIN_WIFI_LED));
      ledTimer = millis() + 750;
    }
  } else {
    //CLient-Mode
    if (WiFi.status() == WL_CONNECTED) {
      //Verbunden -> kurz blinken
      if (millis() > ledTimer) {
        if (digitalRead(PIN_WIFI_LED) == true) {
          digitalWrite(PIN_WIFI_LED, false); ledTimer = millis() + 900;
        } else {
          digitalWrite(PIN_WIFI_LED, true); ledTimer = millis() + 100;
        }
      }
    } else {
        //Nicht Verbunden -> LED aus
        digitalWrite(PIN_WIFI_LED, false);
    }
  }

  //Debug, wenn WLAN-Status geändert
    if (WiFi.status() != wifiStatus) {
        wifiStatus = WiFi.status();
        //WLAN-Status geändert
        Serial.print("WiFi Status: ");
        switch(wifiStatus) {
        case 0:
            Serial.println("WL_IDLE_STATUS");
            break;
        case 1:
            Serial.println("WL_NO_SSID_AVAIL");
            break;
        case 2:
            Serial.println("WL_SCAN_COMPLETED");
            break;
        case 3:
            Serial.println("WL_CONNECTED");
            Serial.printf("IP: %d.%d.%d.%d\n", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]); 
            Serial.printf("NetMask: %d.%d.%d.%d\n", WiFi.subnetMask()[0], WiFi.subnetMask()[1], WiFi.subnetMask()[2], WiFi.subnetMask()[3]); 
            Serial.printf("Gateway: %d.%d.%d.%d\n", WiFi.gatewayIP()[0], WiFi.gatewayIP()[1], WiFi.gatewayIP()[2], WiFi.gatewayIP()[3]); 
            Serial.printf("DNS: %d.%d.%d.%d\n", WiFi.dnsIP()[0], WiFi.dnsIP()[1], WiFi.dnsIP()[2], WiFi.dnsIP()[3]); 
            Serial.printf("Brodcast: %d.%d.%d.%d\n", WiFi.broadcastIP()[0], WiFi.broadcastIP()[1], WiFi.broadcastIP()[2], WiFi.broadcastIP()[3]); 
            break;
        case 4:
            Serial.println("WL_CONNECT_FAILED");
            break;
        case 5:
            Serial.println("WL_CONNECTION_LOST");
            break;
        case 6:
            Serial.println("WL_DISCONNECTED");
            break;
        case 255:
            Serial.println("WL_NO_SHIELD");
            break;
        default:
            Serial.println("UNKNOWN");
        }
    }
}
