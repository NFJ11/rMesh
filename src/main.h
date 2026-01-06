#ifndef MAIN_H
#define MAIN_H

// Pin-Definitionen für T3 V1.6.1
#define LORA_NSS    18
#define LORA_DIO0   26
#define LORA_RST    23
#define LORA_DIO1   33

#define PIN_WIFI_LED 25      //LED WiFi-Status (ein = AP-Mode, blinken = Client-Mode, aus = nicht verbunden)
#define AP_MODE_TASTER 0     //Taster Umschaltung WiFi CLient/AP
#define ANNOUNCE_TIME 5 * 60 * 1000  //ANNOUNCE Baken
#define ANNOUNCE_RANDOM 1 * 60 * 1000   //ANNOUNCE-Zeit + Zufall


struct Peer {
    String call; 
    time_t lastRX;
    float rssi;
    float snr;
    float frqError;
    bool available;
};

void sendPeerList();


#endif
