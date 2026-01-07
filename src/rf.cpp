#include "rf.h"
#include "main.h"
#include <RadioLib.h>
#include "settings.h"

SX1278 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, LORA_DIO1);
#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
bool radioFlag;
bool transmittingFlag;

std::vector<Peer> peerList;
std::vector<Frame> txFrameBuffer;

void setRadioflag(void) {
    radioFlag = true;
}

void printState(int state) {
    if (state == RADIOLIB_ERR_NONE) { Serial.printf("success! \n");} else {Serial.printf("FAILED! code %d\n", state);}
}

void initRadio() {
    //Protokoll
    peerList.reserve(20);
    txFrameBuffer.reserve(10);

    //Flags zurücksetzen
    radioFlag = false;
    transmittingFlag = false;
    int state;

    //Init
    Serial.print("[SX1278] Initializing... ");
    printState(radio.begin());
    Serial.printf("[SX1278] Set SyncWord to 0x%X ... ", settings.loraSyncWord);
    printState(radio.setSyncWord(settings.loraSyncWord));
    Serial.printf("[SX1278] Set Frequency to %f MHz... ", settings.loraFrequency);
    printState(radio.setFrequency(settings.loraFrequency));
    Serial.printf("[SX1278] Set Power to %d dBm ... ", settings.loraOutputPower);
    printState(radio.setOutputPower(settings.loraOutputPower));
    Serial.printf("[SX1278] Set Bandwidth to %f kHz... ", settings.loraBandwidth);
    printState(radio.setBandwidth(settings.loraBandwidth));
    Serial.printf("[SX1278] Set CodingRate to %d ... ", settings.loraCodingRate);
    printState(radio.setCodingRate(settings.loraCodingRate));
    Serial.printf("[SX1278] Set SpreadingFacr to %d ... ", settings.loraSpreadingFactor);
    printState(radio.setSpreadingFactor(settings.loraSpreadingFactor));
    Serial.printf("[SX1278] Set PreambleLength to %d ... ", settings.loraPreambleLength);
    printState(radio.setPreambleLength(settings.loraPreambleLength));

    //"Interrupts"
    radio.setPacketReceivedAction(setRadioflag);
    radio.setPacketSentAction(setRadioflag);

    //RX
    Serial.printf("[SX1278] Starting to listen ... ");
    printState(radio.startReceive());

    //Test PEER eintragen
    Peer p;
    p.lastRX = 0xFFFFFFFF;
    p.call = "NONE";
    p.available = true;
    peerList.push_back(p);

}

bool transmitRAW(uint8_t* data, size_t len) {
    // Prüfen, ob der Kanal frei ist (Channel Activity Detection - CAD)
    if (radio.scanChannel() == RADIOLIB_LORA_DETECTED) {
        //Kanal belegt
        return false;
    } else {
        //Senden
        transmittingFlag = true;
        radio.startTransmit(data, len);

        // //Daten über Websocket senden
        // JsonDocument doc;
        // for (int i = 0; i < len; i++) {
        //   doc["monitor"]["data"][i] = data[i];
        // }
        // doc["monitor"]["tx"] = true;
        // doc["monitor"]["rssi"] = 0;
        // doc["monitor"]["snr"] = 0;
        // doc["monitor"]["frequencyError"] = 0;
        // doc["monitor"]["time"] = time(NULL);
        // String jsonOutput;
        // serializeJson(doc, jsonOutput);
        // ws.textAll(jsonOutput);
    }
    return true;
}


void monitorFrame(Frame &f) {
    //Frame über Websocket senden
    JsonDocument doc;
    for (uint16_t i = 0; i < f.rawDataLength; i++) {
        doc["monitor"]["rawData"][i] = f.rawData[i];
    }
    for (uint16_t i = 0; i < f.messageLength; i++) {
        doc["monitor"]["message"][i] = static_cast<uint8_t>(f.message[i]);
    }    
    doc["monitor"]["tx"] = false;
    doc["monitor"]["rssi"] = radio.getRSSI();
    doc["monitor"]["snr"] = radio.getSNR();
    doc["monitor"]["frequencyError"] = radio.getFrequencyError();
    doc["monitor"]["time"] = f.time;
    doc["monitor"]["srcCall"] = f.srcCall.call;
    doc["monitor"]["srcCallHeader"] = f.srcCall.header;
    doc["monitor"]["dstCall"] = f.dstCall.call;
    doc["monitor"]["dstCallHeader"] = f.dstCall.header;
    doc["monitor"]["frameType"] = f.frameType;
    doc["monitor"]["id"] = f.id;
    doc["monitor"]["initRetry"] = f.initRetry;
    doc["monitor"]["retry"] = f.retry;
    //VIAs
    for (int i = 0; i < f.viaCall.size(); i++) {
        Serial.printf("Call %s\n", f.viaCall[i].call );
        doc["monitor"]["via"][i]["call"] = f.viaCall[i].call;
        doc["monitor"]["via"][i]["header"] = f.viaCall[i].header;
    }    
    String jsonOutput;
    serializeJson(doc, jsonOutput);
    ws.textAll(jsonOutput);
}


bool transmitFrame(Frame &f) {
    //Binärdaten zusammenbauen und senden
    f.rawDataLength = 0;
    //Frame-Typ
    f.rawData[f.rawDataLength] = f.frameType; 
    f.rawDataLength ++;
    //Absender
    f.rawData[f.rawDataLength] = SRC_CALL << 4 | (0x0F & strlen(settings.mycall));  //Header Absender
    f.rawDataLength ++;
    memcpy(&f.rawData[f.rawDataLength], &settings.mycall[0], strlen(settings.mycall)); //Payload
    f.rawDataLength += strlen(settings.mycall);
    //VIAs hinzu
    for (int i = 0; i < f.viaCall.size(); i++) {
        f.rawData[f.rawDataLength] = f.viaCall[i].header << 4 | (0x0F & f.viaCall[i].call.length() );  //Header Absender
        f.rawDataLength ++;
        memcpy(&f.rawData[f.rawDataLength], &f.viaCall[i].call[0], f.viaCall[i].call.length()); //Payload
        f.rawDataLength += f.viaCall[i].call.length();
    }
    //Empfänger hinzu
    if (f.dstCall.call.length() > 0) {
        f.rawData[f.rawDataLength] = DST_CALL << 4 | (0x0F & f.dstCall.call.length());  //Header Absender
        f.rawDataLength ++;
        memcpy(&f.rawData[f.rawDataLength], &f.dstCall.call[0], f.dstCall.call.length()); //Payload
        f.rawDataLength += f.dstCall.call.length();
    }
    //Message hinzu (muss ganz hinten sein)
    if (f.messageLength > 0) {
        //TYP
        f.rawData[f.rawDataLength] = MESSAGE << 4;  //Header TEXT_MESSAGE
        f.rawDataLength ++;
        //ID
        memcpy(&f.rawData[f.rawDataLength], &f.id, sizeof(f.id)); //Payload
        f.rawDataLength += sizeof(f.id);
        //Message
        memcpy(&f.rawData[f.rawDataLength], &f.message[0], f.messageLength); //Payload
        f.rawDataLength += f.messageLength;
    }
    //Bei Frametype TUNE einfach Frame mit 0xFF auffüllen
    if (f.frameType == TUNE) {
        while (f.rawDataLength < 255) {
            f.rawData[f.rawDataLength] = 0xFF;
            f.rawDataLength ++;
        }
    }

    //Frame anpassen
    f.time = time(NULL);
    f.tx = true;
    f.srcCall.call = settings.mycall;
    f.srcCall.header = SRC_CALL;
    f.dstCall.header = DST_CALL;

    //Senden
    monitorFrame(f);
    return transmitRAW(f.rawData, f.rawDataLength);
}

void sendMessage(String dstCall, String text) {
    //Neuen Frame zusammenbauen
    Frame f;
    f.frameType = TEXT_MESSAGE;
    f.dstCall.call = dstCall;
    text.toCharArray(f.message, 255);
    f.messageLength = text.length();
    f.retry = TX_RETRY;
    f.initRetry = f.retry;
    f.id = millis();
    f.transmitMillis = 0;
    //VIA-Calls dazu
    for (int i = 0; i < peerList.size(); i++) {
        if (peerList[i].available) {
            CallsignWithHeader c;
            c.call = peerList[i].call;
            c.header = VIA_REPEAT;
            f.viaCall.push_back(c);
        }
    } 
    //Frame in Sendebuffer
    txFrameBuffer.push_back(f);
}


void sendPeerList() {
  JsonDocument doc;
  for (int i = 0; i < peerList.size(); i++) {
    //Serial.printf("Peer List #%d %s\n", i, peerList[i].call);
    doc["peerlist"]["peers"][i]["call"] = peerList[i].call;
    doc["peerlist"]["peers"][i]["lastRX"] = peerList[i].lastRX;
    doc["peerlist"]["peers"][i]["rssi"] = peerList[i].rssi;
    doc["peerlist"]["peers"][i]["snr"] = peerList[i].snr;
    doc["peerlist"]["peers"][i]["frqError"] = peerList[i].frqError;
    doc["peerlist"]["peers"][i]["available"] = peerList[i].available;
  }  
  doc["peerlist"]["count"] = peerList.size();
  String jsonOutput;
  serializeJson(doc, jsonOutput);
  ws.textAll(jsonOutput);
}


void addPeerList(Peer p) {
    //In Peers suchen
    bool add = true;
    for (int i = 0; i < peerList.size(); i++) {
        //Serial.printf("Add Peer List %i %s %s\n", i, peerList[i].call, p.call);
        if (peerList[i].call == p.call) {
            add = false;
            //Peer-List updaten bis auf Available Flag
            bool availableOld = peerList[i].available;
            peerList[i] = p;
            peerList[i].available = availableOld;
            break;
        }
    }    
    //Nicht grunfen -> hinzufügen (immer ohne Available Flag )
    if (add) {
        p.available = false;
        peerList.push_back(p);
    }
    sendPeerList();
}

void availablePeerList(String call, bool available) {
    //Available Flag in Peer-Liste setzen
    for (int i = 0; i < peerList.size(); i++) {
        if (peerList[i].call == call) {
            peerList[i].available = available;
        }
    }
    sendPeerList();
}

void checkPeerList() {
    //Peer-Liste bereinigen
    for (int i = 0; i < peerList.size(); i++) {
        time_t age = time(NULL) - peerList[i].lastRX;
        if (age > PEER_TIMEOUT) {
            peerList.erase(peerList.begin() + i);
            sendPeerList();
            break;
        }
    }    
}


void addSourceCall(uint8_t* data, uint8_t &len) {
    data[len] = 0x00 | (0x0F & strlen(settings.mycall));  //Header Absender
    len ++;
    memcpy(&data[len], &settings.mycall[0], strlen(settings.mycall)); //Payload
    len += strlen(settings.mycall);
}


// // initialize SX1278 with default settings
//   Serial.print(F("[SX1278] Initializing ... "));

//   int state = radio.begin();
//   if (state == RADIOLIB_ERR_NONE) {
//     Serial.println(F("success!"));
//   } else {
//     Serial.print(F("failed, code "));
//     Serial.println(state);
//     while (true) { delay(10); }
//   }

   

//   state = radio.scanChannel();

//   if (state == RADIOLIB_ERR_NONE) {
//       Serial.println(F("Sendeleistung erfolgreich gesetzt!"));
//   } else {
//       Serial.print(F("Fehler beim Setzen der Leistung, Code: "));
//       Serial.println(state);
//   }

// state = radio.setPreambleLength(8);

// if (state == RADIOLIB_ERR_NONE) {
//     Serial.println(F("Preamble erfolgreich gesetzt!"));
// } else {
//     Serial.print(F("Fehler beim Setzen der Preamble, Code: "));
//     Serial.println(state);
// }


//   float neueFrequenz = 433.175;
//   state = radio.setFrequency(neueFrequenz);

//   if (state == RADIOLIB_ERR_NONE) {
//       Serial.print(F("Frequenz gewechselt auf: "));
//       Serial.println(neueFrequenz);
//   } else {
//       Serial.print(F("Fehler beim Frequenzwechsel, Code: "));
//       Serial.println(state);
//   }

// state = radio.setBandwidth(250.0);

// if (state == RADIOLIB_ERR_NONE) {
//     Serial.println(F("Bandbreite erfolgreich auf 250 kHz gesetzt"));
// } else {
//     Serial.print(F("Fehler beim Setzen der Bandbreite, Code: "));
//     Serial.println(state);
// }

//   // Nachträglich im Setup setzen
//    state = radio.setSyncWord(0x2b);
//   if (state == RADIOLIB_ERR_NONE) {
//       Serial.println(F("Sync Word erfolgreich gesetzt"));
//   } else {
//       Serial.print(F("Fehler: "));
//       Serial.println(state);
//   }

//    state = radio.setCodingRate(6); 

//   if (state == RADIOLIB_ERR_NONE) {
//       Serial.println(F("Coding Rate erfolgreich gesetzt!"));
//   } else {
//       Serial.print(F("Fehler: "));
//       Serial.println(state);
//   }

//   // Setzt den Spreading Factor auf 9 (MeshCom Standard)
//  state = radio.setSpreadingFactor(11);

// if (state == RADIOLIB_ERR_NONE) {
//     Serial.println(F("SF erfolgreich gesetzt!"));
// } else {
//     // Falls der Wert außerhalb des Bereichs liegt (z.B. < 6 oder > 12)
//     Serial.print(F("Fehler beim Setzen des SF, Code: "));
//     Serial.println(state);
// }





//   // set the function that will be called
//   // when new packet is received
//   radio.setPacketReceivedAction(setRxFlag);
//   radio.setPacketSentAction(setRxFlag);

//   // start listening for LoRa packets
//   Serial.print(F("[SX1278] Starting to listen ... "));
//   state = radio.startReceive();
//   if (state == RADIOLIB_ERR_NONE) {
//     Serial.println(F("success!"));
//   } else {
//     Serial.print(F("failed, code "));
//     Serial.println(state);
//     while (true) { delay(10); }
//   }

//   // if needed, 'listen' mode can be disabled by calling
//   // any of the following methods:
//   //
//   // radio.standby()
//   // radio.sleep()
//   // radio.transmit();
//   // radio.receive();
//   // radio.scanChannel();  



// if (state == RADIOLIB_LORA_DETECTED) {
//     // Kanal ist belegt!
//     Serial.println(F("Besetzt! Senden verschoben."));
//   } else {
//     // Kanal ist frei
//     Serial.println(F("Frei! Sende jetzt..."));
//     radio.startTransmit("Hello World!");
//   }

//   // you can also transmit byte array up to 255 bytes long
//   /*
//     byte byteArr[] = {0x01, 0x23, 0x45, 0x67,
//                       0x89, 0xAB, 0xCD, 0xEF};
//     transmissionState = radio.startTransmit(byteArr, 8);
//   */

  



// void loop() {
//   // put your main code here, to run repeatedly:




//   if(receivedFlag) {
//     // reset flag
//     receivedFlag = false;

//     // you can read received data as an Arduino String
//     String str;
//     int state = radio.readData(str);

//     // you can also read received data as byte array
//     /*
//       byte byteArr[8];
//       int numBytes = radio.getPacketLength();
//       int state = radio.readData(byteArr, numBytes);
//     */

//     if (state == RADIOLIB_ERR_NONE) {
//       // packet was successfully received
//       Serial.println(F("[SX1278] Received packet!"));

//       // print data of the packet
//       Serial.print(F("[SX1278] Data:\t\t"));
//       Serial.println(str);

//       // print RSSI (Received Signal Strength Indicator)
//       Serial.print(F("[SX1278] RSSI:\t\t"));
//       Serial.print(radio.getRSSI());
//       Serial.println(F(" dBm"));

//       // print SNR (Signal-to-Noise Ratio)
//       Serial.print(F("[SX1278] SNR:\t\t"));
//       Serial.print(radio.getSNR());
//       Serial.println(F(" dB"));

//       // print frequency error
//       Serial.print(F("[SX1278] Frequency error:\t"));
//       Serial.print(radio.getFrequencyError());
//       Serial.println(F(" Hz"));

//     } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
//       // packet was received, but is malformed
//       Serial.println(F("[SX1278] CRC error!"));

//     } else {
//       // some other error occurred
//       Serial.print(F("[SX1278] Failed, code "));
//       Serial.println(state);

//     }

//   radio.startReceive();

//   }
// }

