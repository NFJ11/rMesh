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
uint32_t statusTimer = millis();
uint32_t rebootTimer = 0xFFFFFFFF;
uint32_t txPauseTimer = 0;



void printHexArray(uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    if (data[i] < 0x10) {
      Serial.print("0"); // Führende Null für Werte kleiner als 16
    }
    Serial.print(data[i], HEX); 
    Serial.print(" "); // Leerzeichen zur besseren Lesbarkeit
  }
  Serial.println(); // Zeilenumbruch am Ende
}

void limitFileLines(const char* path, int maxLines) {
    File file = LittleFS.open(path, "r");
    if (!file) return;
   int count = 0;
    while (file.available()) {
        if (file.read() == '\n') count++;
    }
    if (count <= maxLines) {
        file.close();
        return;
    }
    int skipLines = count - (maxLines - 50); // Wir behalten 950, um Puffer zu haben
    file.seek(0);
    File tempFile = LittleFS.open("/temp.json", "w");
    int currentLine = 0;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        if (currentLine >= skipLines) {
            tempFile.println(line);
        }
        currentLine++;
    }
    file.close();
    tempFile.close();
    LittleFS.remove(path);
    LittleFS.rename("/temp.json", path);
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
	if ((transmittingFlag == false) && (millis() > txPauseTimer)) {
		//Sendepuffer duchlaufen
		//for (int i = 0; i < txFrameBuffer.size(); i++) {
        if (txFrameBuffer.size() > 0) {
            int i = 0;
			//Prüfen, ob Frame gesendet werden muss
			if ((millis() > txFrameBuffer[i].transmitMillis) && (txFrameBuffer[i].suspendTX == false)) {
				//Frame senden
                if (transmitFrame(txFrameBuffer[i])) {
					//Erfolg
					//Retrys runterzählen
					txFrameBuffer[i].retry --;
					//Nächsten Sendezeitpunkt festlegen
					txFrameBuffer[i].transmitMillis = millis() + TX_RETRY_TIME;
					//Wenn kein Retry mehr übrig, dann löschen
					if (txFrameBuffer[i].retry == 0) {
                        //Aus Peer-Liste löschen
                        availablePeerList(txFrameBuffer[i].viaCall.call, false);
                        //Falls weitere Frames im TX-Puffer mit der gleichen MSG ID sind -> den ersten aktivieren
                        for (int ii = 0; ii < txFrameBuffer.size(); ii++) {
                            if ((txFrameBuffer[ii].id == txFrameBuffer[i].id) && (txFrameBuffer[ii].suspendTX == true)){
                                txFrameBuffer[ii].suspendTX = false;
                                txFrameBuffer[ii].transmitMillis = millis() + TX_RETRY_TIME + getLoRaToA(20, settings.loraSpreadingFactor, settings.loraBandwidth, settings.loraCodingRate, settings.loraPreambleLength);  //Zeit für ACK Frame
                            }
                        }
                        //Frame löschen
                        txFrameBuffer.erase(txFrameBuffer.begin() + i);
					}
				} else {
					//Fehler beim Senden -> Später nochmal
					txFrameBuffer[i].transmitMillis = millis() + TX_PAUSE_TIME;
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
			Frame rxFrame;
            rxFrame.rawDataLength = radio.getPacketLength();
			int16_t state = radio.readData(rxFrame.rawData, rxFrame.rawDataLength);
			if (state == RADIOLIB_ERR_NONE) {
                //Serial.printf("RX Length: %d\n", rxFrame.rawDataLength);
                //printHexArray(rxFrame.rawData, rxFrame.rawDataLength);
				//Empfangene Daten in Frame parsen
				rxFrame.time = time(NULL);
				rxFrame.rssi = radio.getRSSI();
				rxFrame.snr = radio.getSNR();
				rxFrame.frqError =  radio.getFrequencyError();
				rxFrame.tx = false;
				//Frametype
				rxFrame.frameType = rxFrame.rawData[0];
				//Frame druchlaufen und nach Headern suchen
				uint8_t header = 0;
				uint8_t length = 0;
				for (uint8_t i=1; i < rxFrame.rawDataLength; i++) {
					//Header prüfen
					header = rxFrame.rawData[i] >> 4;
					switch (header) {
                        case SRC_CALL:	
                        case DST_CALL:	
                        case VIA_CALL:	
                        case NODE_CALL:	
							length = (rxFrame.rawData[i] & 0x0F);
                            //Serial.printf("Call detected\n");
							//max. Länge prüfen
							if (length > MAX_CALLSIGN_LENGTH) {length = MAX_CALLSIGN_LENGTH;}
							//nicht außerhalb vom rxBuffer lesen
							if ((i + length) < rxFrame.rawDataLength) {
								//Schleife von aktueller Position + 1 bis "length"
								for(int ii = i + 1; ii <= i + length; ii++) {
									//Einzelne Bytes in String kopieren
                                    switch (header) {
                                        case SRC_CALL : rxFrame.srcCall.call += (char)rxFrame.rawData[ii]; break;
                                        case DST_CALL : rxFrame.dstCall.call += (char)rxFrame.rawData[ii]; break;
                                        case VIA_CALL : rxFrame.viaCall.call += (char)rxFrame.rawData[ii]; break;
                                        case NODE_CALL : rxFrame.nodeCall.call += (char)rxFrame.rawData[ii]; break;
                                    }
								}
							}
							//Zum nächsten Header springen (wenn Frame lange genug)
							if ((i + length) <= rxFrame.rawDataLength) {i += length;}
							break;
						case MESSAGE:
							//Nur beim passenden Frame-Type
							if ((rxFrame.frameType == FrameType::TEXT_MESSAGE) || (rxFrame.frameType == FrameType::MESSAGE_ACK))  {
								//ID ausschneiden
								if (rxFrame.rawDataLength >= (i + sizeof(rxFrame.id))) {
									rxFrame.id = (rxFrame.rawData[i + 4] << 24) + (rxFrame.rawData[i + 3] << 16) + (rxFrame.rawData[i + 2] << 8) + rxFrame.rawData[i + 1];  
									i += sizeof(rxFrame.id) + 1;
								}
								//Message Länge
								rxFrame.messageLength = rxFrame.rawDataLength - i;
								//Message
								memcpy(rxFrame.message, rxFrame.rawData + i, rxFrame.messageLength);
								//Suche beenden
								i = rxFrame.rawDataLength;
							}
					}
				}

				//In Peer-Liste einfügen
				Peer p;
				p.call = rxFrame.nodeCall.call;
				p.lastRX = time(NULL);
				p.frqError = radio.getFrequencyError();
				p.rssi = radio.getRSSI();
				p.snr = radio.getSNR();
				addPeerList(p);

				//Frame an Monitor senden
				monitorFrame(rxFrame);

				//Frame Typ prüfen & Antwort basteln
				Frame txFrame;
				bool txAbort = false;
				switch (rxFrame.frameType) {
				case FrameType::ANNOUNCE:  //Announce 
					//Antowrt zusammenbauen
                    if (rxFrame.nodeCall.call.length() > 0) {
                        txFrame.frameType = FrameType::ANNOUNCE_REPLY;
                        txFrame.transmitMillis = millis() + ACK_TIME;
                        txFrame.dstCall.call = rxFrame.nodeCall.call;
                        txFrame.retry = 1;
                        txFrameBuffer.push_back(txFrame);
                    }
					break;          
				case FrameType::ANNOUNCE_REPLY:  //Announce REPLAY
                    if (rxFrame.dstCall.call == String(settings.mycall)) {
                        availablePeerList(rxFrame.nodeCall.call, true);
                    }
					break;
				case MESSAGE_ACK: //Senden abbrechen
					//Im TX-Puffer nach MSG-ID und NODE-Call suchen und löschen
                    // Serial.printf("MESSAGE_ACK\n");
					for (int i = 0; i < txFrameBuffer.size(); i++) {
						if ((txFrameBuffer[i].id == rxFrame.id) && (txFrameBuffer[i].viaCall.call == rxFrame.nodeCall.call)) {
                            //In Peer Liste eintragen
                            availablePeerList(rxFrame.nodeCall.call, true);
                            //TX Puffer löschen
                            txFrameBuffer.erase(txFrameBuffer.begin() + i); 
                            //Falls weitere Frames im TX-Puffer mit der gleichen MSG ID sind -> den ersten aktivieren
                            for (int ii = 0; ii < txFrameBuffer.size(); ii++) {
                                if ((txFrameBuffer[ii].id == rxFrame.id) && (txFrameBuffer[ii].suspendTX == true)){
                                    txFrameBuffer[ii].suspendTX = false;
                                    txFrameBuffer[ii].transmitMillis = millis() + TX_RETRY_TIME;
                                }
                            }
						}
					}
					break;
				case FrameType::TEXT_MESSAGE:  	//TEXT Message
                    //ACK-Senden
                    Frame r;
                    r.frameType = MESSAGE_ACK;
                    r.dstCall = rxFrame.srcCall;
                    r.retry = 1;
                    r.tx = 1;
                    r.id = rxFrame.id;
                    r.transmitMillis = millis() + ACK_TIME;
                    txFrameBuffer.push_back(r);

                    //Message ID und SCR-Call in Datei suchen
                    File fileRead = LittleFS.open("/messages.json", "r");
                    bool found = false;
                    if (fileRead) {
                        JsonDocument doc;
                        while (fileRead.available()) {
                            DeserializationError error = deserializeJson(doc, fileRead);
                            if (error == DeserializationError::Ok) {
                                if ((doc["message"]["id"].as<uint32_t>() == rxFrame.id) && (doc["message"]["srcCall"].as<String>() == rxFrame.srcCall.call) && (doc["message"]["tx"] .as<bool>() == false)) {
                                    found = true;
                                    break; 
                                }
                            } else if (error != DeserializationError::EmptyInput) {
                                fileRead.readStringUntil('\n');
                            }
                        }
                        fileRead.close();                    
                    }

                    //Message über Websocket senden & speichern
                    if (found == false) {
                        //Message über Websocket senden
                        String json = messageJson(String((char*)rxFrame.message).substring(0, rxFrame.messageLength), rxFrame.srcCall.call, rxFrame.dstCall.call, rxFrame.nodeCall.call, rxFrame.time, rxFrame.id, false);
                        ws.textAll(json);

                        //Message in Datei speichern
                        File file = LittleFS.open("/messages.json", "a"); 
                        if (file) {
                            file.println(json); 
                            file.close();
                            limitFileLines("/messages.json", MAX_STORED_MESSAGES);
                        }
                    }
					break;
				}
			}  
	  		//Empfang wieder starten
			radio.startReceive();
		}

		//Senden fertig
		if (irqFlags == 0x08) {
			transmittingFlag = false;
            statusTimer = 0;
            txPauseTimer = millis() + TX_PAUSE_TIME;
			radio.startReceive();
		}
	}


  //Status über Websocket senden
  if (millis() > statusTimer) {
	statusTimer = millis() + 1000;

	//Zeit über Websocket senden
	JsonDocument doc;
	doc["status"]["time"] = time(NULL);
	doc["status"]["tx"] = transmittingFlag;
	doc["status"]["txBufferCount"] = txFrameBuffer.size();
	String jsonOutput;
	serializeJson(doc, jsonOutput);
	ws.textAll(jsonOutput);

	//Peer-Liste checken
	checkPeerList();

  }

  //Reboot
  if (millis() > rebootTimer) {ESP.restart();}
}