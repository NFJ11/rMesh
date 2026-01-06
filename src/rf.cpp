#include "rf.h"
#include "main.h"
#include <RadioLib.h>
#include "settings.h"

SX1278 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, LORA_DIO1);
#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
bool radioFlag;

void setRadioflag(void) {
    radioFlag = true;
}

void printState(int state) {
    if (state == RADIOLIB_ERR_NONE) { Serial.printf("success! \n");} else {Serial.printf("FAILED! code %d\n", state);}
}

void initRadio() {
    //Flags zurücksetzen
    radioFlag = false;
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

}

bool transmit(uint8_t* data, size_t len) {
    // Prüfen, ob der Kanal frei ist (Channel Activity Detection - CAD)
    if (radio.scanChannel() == RADIOLIB_LORA_DETECTED) {
        //Kanal belegt
        return false;
    } else {
        //Senden
        radio.startTransmit(data, len);
        //Daten über Websocket senden
        JsonDocument doc;
        for (int i = 0; i < len; i++) {
          doc["monitor"]["data"][i] = data[i];
        }
        doc["monitor"]["tx"] = true;
        doc["monitor"]["rssi"] = 0;
        doc["monitor"]["snr"] = 0;
        doc["monitor"]["frequencyError"] = 0;
        doc["monitor"]["time"] = time(NULL);
        String jsonOutput;
        serializeJson(doc, jsonOutput);
        ws.textAll(jsonOutput);
    }
    return true;
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

