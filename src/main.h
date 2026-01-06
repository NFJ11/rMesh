#ifndef MAIN_H
#define MAIN_H

// Pin-Definitionen für T3 V1.6.1
#define LORA_NSS    18
#define LORA_DIO0   26
#define LORA_RST    23
#define LORA_DIO1   33
#define PIN_WIFI_LED 25      //LED WiFi-Status (ein = AP-Mode, blinken = Client-Mode, aus = nicht verbunden)
#define AP_MODE_TASTER 0     //Taster Umschaltung WiFi CLient/AP

//Timing
#define ANNOUNCE_TIME 5 * 60 * 1000 + random(0, 1 * 60 * 1000)  //ANNOUNCE Baken
#define ANNOUNCE_REPLAY_TIME random(0, 5000)                    //Pause für Antwort auf Announce
#define PEER_TIMEOUT 15 * 60 * 1000 //Zeit, nach dem ein Call aus der Peer-Liste gelöscht wird
#define TX_BUSY_RETRY 500 //Zeit, nach der nochmal gesendet wird, wenn QRG besetzt



#endif
