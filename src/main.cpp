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
uint32_t announceTimer = 5000; //Erstes Announce nach 5 Sekunden

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
		announceTimer = millis() + ANNOUNCE_TIME;
		Frame f;
		f.frameType = FrameType::ANNOUNCE;
		f.transmitMillis = 0;
		f.retry = 1;
		//Frame in SendeBuffer
		txFrameBuffer.push_back(f);
	}
  
	//Prüfen, ob was gesendet werden muss
	if (transmittingFlag == false) {
		//Sendepuffer duchlaufen
		for (int i = 0; i < txFrameBuffer.size(); i++) {
			//Prüfen, ob Frame gesendet werden muss
			if (millis() > txFrameBuffer[i].transmitMillis) {
				//Frame senden
				uint32_t a;
				if (transmitFrame(txFrameBuffer[i])) {
					//Erfolg
					//Retrys runterzählen
					txFrameBuffer[i].retry --;
					//Nächsten Sendezeitpunkt festlegen
					txFrameBuffer[i].transmitMillis = millis() + TX_RETRY_TIME;
					//Wenn kein Retry mehr übrig, dann löschen
					if (txFrameBuffer[i].retry == 0) {
						//Call, mit VIA_REPEAT in Peers auf nicht available setzen
						if (txFrameBuffer[i].initRetry > 1) {
							for (int ii = 0; ii < txFrameBuffer[i].viaCall.size(); ii++) {
								if (txFrameBuffer[i].viaCall[ii].header == VIA_REPEAT) {
									availablePeerList(txFrameBuffer[i].viaCall[ii].call, false);
								}
							}
						}
						txFrameBuffer.erase(txFrameBuffer.begin() + i);
					}
				} else {
					//Fehler beim Senden -> Später nochmal
					txFrameBuffer[i].transmitMillis = millis() + TX_BUSY_RETRY_TIME;
				}
			}    
		}
	}


	//Prüfen ob Änderungen vom HF-Chip vorliegen
	if (radioFlag) {
		radioFlag = false;
		//"Echte" Flags auslesen
		uint16_t irqFlags = radio.getIRQFlags();
		
		//Daten Empfangen
		if (irqFlags == 0x50) {
			//Prüfen, ob was empfangen wurde
			byte rxBytes[256];
			size_t rxSize = radio.getPacketLength();
			int16_t state = radio.readData(rxBytes, rxSize);
			if (state == RADIOLIB_ERR_NONE) {
				//Empfangene Daten in Frame parsen
				Frame rxFrame;
				rxFrame.viaCall.reserve(6);
				memcpy(rxFrame.rawData, rxBytes, 255);
				rxFrame.rawDataLength = rxSize;
				rxFrame.time = time(NULL);
				rxFrame.rssi = radio.getRSSI();
				rxFrame.snr = radio.getSNR();
				rxFrame.frqError =  radio.getFrequencyError();
				rxFrame.tx = false;
				//Frametype
				rxFrame.frameType = rxBytes[0];
				//Frame druchlaufen und nach Headern suchen
				uint8_t header = 0;
				uint8_t length = 0;
				String viaCall;
				CallsignWithHeader cWH;
				for (uint8_t i=1; i < rxSize; i++) {
					//Header prüfen
					header = rxBytes[i] >> 4;
					switch (header) {
						case SRC_CALL:	
							length = (rxBytes[i] & 0x0F);
							//max. Länge prüfen
							if (length > MAX_CALLSIGN_LENGTH) {length = MAX_CALLSIGN_LENGTH;}
							//nicht außerhalb vom rxBuffer lesen
							if ((i + length) < rxSize) {
								//Schleife von aktueller Position + 1 bis "length"
								for(int ii = i + 1; ii <= i + length; ii++) {
									//Einzelne Bytes in String kopieren
									rxFrame.srcCall.call += (char)rxBytes[ii]; 
								}
							}
							//Zum nächsten Header springen (wenn Frame lange genug)
							if ((i + length) <= rxSize) {i += length;}
							break;
						case DST_CALL:
							length = (rxBytes[i] & 0x0F);
							//max. Länge prüfen
							if (length > MAX_CALLSIGN_LENGTH) {length = MAX_CALLSIGN_LENGTH;}
							//nicht außerhalb vom rxBuffer lesen
							if ((i + length) < rxSize) {
								//Schleife von aktueller Position + 1 bis "length"
								for(int ii = i + 1; ii <= i + length; ii++) {
									//Einzelne Bytes in String kopieren
									rxFrame.dstCall.call += (char)rxBytes[ii]; 
								}
							}
							//Zum nächsten Header springen (wenn Frame lange genug)
							if ((i + length) <= rxSize) {i += length;}
							break;
						case VIA_REPEAT:
						case VIA_NO_REPEAT:
							length = (rxBytes[i] & 0x0F);
							//max. Länge prüfen
							if (length > MAX_CALLSIGN_LENGTH) {length = MAX_CALLSIGN_LENGTH;}
							//nicht außerhalb vom rxBuffer lesen
							if ((i + length) < rxSize) {
								//Schleife von aktueller Position + 1 bis "length"
								viaCall = "";
								for(int ii = i + 1; ii <= i + length; ii++) {
									//Einzelne Bytes in String kopieren
									viaCall += (char)rxBytes[ii]; 
								}
								cWH.call = viaCall;
								cWH.header = header;
								rxFrame.viaCall.push_back(cWH);
							}
							//Zum nächsten Header springen (wenn Frame lange genug)
							if ((i + length) <= rxSize) {i += length;}
							break;
						case MESSAGE:
							//Nur beim passenden Frame-Type
							if ((rxFrame.frameType == FrameType::TEXT_MESSAGE) || (rxFrame.frameType == FrameType::MESSAGE_REPLY))  {
								//ID ausschneiden
								if (rxSize >= (i + sizeof(rxFrame.id))) {
									rxFrame.id = (rxBytes[i + 4] << 24) + (rxBytes[i + 3] << 16) + (rxBytes[i + 2] << 8) + rxBytes[i + 1];  
									i += sizeof(rxFrame.id) + 1;
								}
								//Message Länge
								rxFrame.messageLength = rxSize - i;
								//Message
								memcpy(rxFrame.message, rxBytes + i, rxFrame.messageLength);
								//Suche beenden
								i = rxSize;
							}
					}
				}

				//In Peer-Liste einfügen
				Peer p;
				p.call = rxFrame.srcCall.call;
				p.lastRX = time(NULL);
				p.frqError = radio.getFrequencyError();
				p.rssi = radio.getRSSI();
				p.snr = radio.getSNR();
				addPeerList(p);

				//Falls ANNOUNCE_REPLY empfangen wurde, dann Available = true
				if ((rxFrame.dstCall.call == String(settings.mycall)) && (rxFrame.frameType == ANNOUNCE_REPLY))  {
					availablePeerList(rxFrame.srcCall.call, true);
				}

				//Frame an Monitor senden
				monitorFrame(rxFrame);

				//Frame Typ prüfen & Antwort basteln
				Frame txFrame;
				bool txAbort = false;
				switch (rxFrame.frameType) {
				case FrameType::ANNOUNCE:  //Announce 
					//Antowrt zusammenbauen
					txFrame.frameType = FrameType::ANNOUNCE_REPLY;
					txFrame.transmitMillis = millis() + ANNOUNCE_REPLAY_TIME;
					txFrame.dstCall.call = rxFrame.srcCall.call;
					txFrame.retry = 1;
					txFrameBuffer.push_back(txFrame);
					break;          
				case FrameType::ANNOUNCE_REPLY:  //Announce REPLAY
					//Wird oben bei der Peer-Liste abgehandelt
					break;
				case MESSAGE_REPLY: //Senden abbrechen
					//viaR HEADER nach via HEADER setzen
					txAbort = true;
					for (int i = 0; i < txFrameBuffer.size(); i++) {
						if (txFrameBuffer[i].id == rxFrame.id) {
							for (int ii = 0; ii < txFrameBuffer[i].viaCall.size(); ii++) {
								if ((txFrameBuffer[i].viaCall[ii].header == VIA_REPEAT) && (txFrameBuffer[i].viaCall[ii].call) == rxFrame.srcCall.call) {
									txFrameBuffer[i].viaCall[ii].header = VIA_NO_REPEAT;
								}
								//Prüfen, ob noch Repeat-Header da sind
								if (txFrameBuffer[i].viaCall[ii].header == VIA_REPEAT) {txAbort = false;}
							}	
							if (txAbort == true) {  txFrameBuffer.erase(txFrameBuffer.begin() + i); };					
						}
					}
					break;
				case FrameType::TEXT_MESSAGE:  	//TEXT Message
					//Antwort senden
					//Prüfen, ob eigenes Call in VIA-Liste
					for (int i = 0; i < rxFrame.viaCall.size(); i++) {
						//Serial.printf("Call %s\n", f.viaCall[i].call );
        				if (rxFrame.viaCall[i].call == String(settings.mycall)) {
							//Antwort senden
							Frame r;
							r.frameType = MESSAGE_REPLY;
							r.dstCall = rxFrame.srcCall;
							r.retry = 1;
							r.tx = 1;
							r.id = rxFrame.id;
							r.messageLength = 1;
							r.message[0] = 0x00;
							r.transmitMillis = millis() + 1000; // ************************************** ANPASSEN AN POSITION IN VIA LISTE
							txFrameBuffer.push_back(r);
						}
					}  




					//Message über Websocket senden  ***************************************************  NUR WENN MSGID NOCH NICHT IM CACHE
					JsonDocument doc;
					doc["message"]["message"] = String((char*)rxFrame.message).substring(0, rxFrame.messageLength);
					doc["message"]["srcCall"] = rxFrame.srcCall.call;
					doc["message"]["dstCall"] = rxFrame.dstCall.call;
					doc["message"]["time"] = rxFrame.time;
					String jsonOutput;
					serializeJson(doc, jsonOutput);
					ws.textAll(jsonOutput);



										//ID + CALL IN MESSAGE speicher (wegen doppelt)



										File file = LittleFS.open("/messages.json", "a"); 
					if (!file) {
						Serial.println("Fehler beim Öffnen");
						return;
					}

    // JSON in die Datei schreiben
    serializeJson(doc, file);
    // WICHTIG: Zeilenumbruch für den nächsten Datensatz
    file.println(); 
    
    file.close();

					break;
				}
			}  
	  		//Empfang wieder starten
			radio.startReceive();
		}

		//Senden fertig
		if (irqFlags == 0x08) {
			transmittingFlag = false;
			radio.startReceive();
		}
	}


  //Status über Websocket senden
  if (millis() > webSocketTimer) {
	webSocketTimer = millis() + 1000;

	//Zeit über Websocket senden
	JsonDocument doc;
	doc["time"] = time(NULL);
	String jsonOutput;
	serializeJson(doc, jsonOutput);
	ws.textAll(jsonOutput);

	//Peer-Liste checken
	checkPeerList();

  }

  //Reboot
  if (millis() > rebootTimer) {ESP.restart();}
}