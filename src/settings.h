#include <Arduino.h>

//Konfiguration
struct Settings {
  bool dhcpActive;
  bool apMode;
  char wifiSSID[64];
  char wifiPassword[64];
  char name[64];
  IPAddress wifiIP;
  IPAddress wifiNetMask;
  IPAddress wifiGateway;
  IPAddress wifiDNS;
};
extern Settings settings;

void defaultSettings();
void saveSettings();
void loadSettings();
bool checkSettings();
void showSettings();