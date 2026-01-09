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
#define PEER_TIMEOUT 15 * 60 * 1000             //Zeit, nach dem ein Call aus der Peer-Liste gelöscht wird
#define ACK_TIME 500 + random(0, 1000)           //Zeit, bis ein ACK gesendet wird
#define TX_PAUSE_TIME 1000                       //Minimale Pause zwischen 2 Aussendungen
#define TX_RETRY 5                              //Retrys beim Senden 
#define TX_RETRY_TIME 3000 + random(0, 2000)    //Pause zwischen wiederholungen (muss größer als ACK_TIME sein + Message Länge)
#define MAX_STORED_MESSAGES 500                //max. in "messages.json" gespeicherte Nachrichten


//Interner Quatsch
#define NAME "rMesh"                        //Versions-String
#define VERSION "V0.1"                        //Versions-String
#define MAX_CALLSIGN_LENGTH 8                  //maximale Länge des Rufzeichens

extern uint32_t statusTimer;
void limitFileLines(const char* path, int maxLines);

#endif
