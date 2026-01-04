#include <Arduino.h>
#include <RadioLib.h>
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


//RadioLib config
SX1278 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, LORA_DIO1);
bool receivedFlag = false;
#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif


void setRxFlag(void) {
  // we got a packet, set the flag
  receivedFlag = true;
}

bool transmittedFlag = false;
void setTxFlag(void) {
  // we sent a packet, set the flag
  transmittedFlag = true;
}

void setup() {
  setCpuFrequencyMhz(240);

  //SPI Init
  SPI.begin(5, 19, 27, 18);

  //Debug-Port
  Serial.begin(115200);
  //Serial.setDebugOutput(true);  

  //Einstellungen laden
  loadSettings();
  if (checkSettings() == false) {
    defaultSettings();
    saveSettings();
  }
  defaultSettings();
  showSettings();

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
  } else {
    Serial.println("LittleFS mounted successfully");
  } 

  //WEB-Server starten
  //startWebServer();

  // initialize SX1278 with default settings
  Serial.print(F("[SX1278] Initializing ... "));

  int state = radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  state = radio.setOutputPower(17);


  state = radio.scanChannel();
  if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("Sendeleistung erfolgreich gesetzt!"));
  } else {
      Serial.print(F("Fehler beim Setzen der Leistung, Code: "));
      Serial.println(state);
  }

state = radio.setPreambleLength(8);

if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Preamble erfolgreich gesetzt!"));
} else {
    Serial.print(F("Fehler beim Setzen der Preamble, Code: "));
    Serial.println(state);
}


  float neueFrequenz = 433.175;
  state = radio.setFrequency(neueFrequenz);

  if (state == RADIOLIB_ERR_NONE) {
      Serial.print(F("Frequenz gewechselt auf: "));
      Serial.println(neueFrequenz);
  } else {
      Serial.print(F("Fehler beim Frequenzwechsel, Code: "));
      Serial.println(state);
  }

state = radio.setBandwidth(250.0);

if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Bandbreite erfolgreich auf 250 kHz gesetzt"));
} else {
    Serial.print(F("Fehler beim Setzen der Bandbreite, Code: "));
    Serial.println(state);
}

  // Nachträglich im Setup setzen
   state = radio.setSyncWord(0x2b);
  if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("Sync Word erfolgreich gesetzt"));
  } else {
      Serial.print(F("Fehler: "));
      Serial.println(state);
  }

   state = radio.setCodingRate(6); 

  if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("Coding Rate erfolgreich gesetzt!"));
  } else {
      Serial.print(F("Fehler: "));
      Serial.println(state);
  }

  // Setzt den Spreading Factor auf 9 (MeshCom Standard)
 state = radio.setSpreadingFactor(11);

if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("SF erfolgreich gesetzt!"));
} else {
    // Falls der Wert außerhalb des Bereichs liegt (z.B. < 6 oder > 12)
    Serial.print(F("Fehler beim Setzen des SF, Code: "));
    Serial.println(state);
}





  // set the function that will be called
  // when new packet is received
  radio.setPacketReceivedAction(setRxFlag);
  radio.setPacketSentAction(setRxFlag);

  // start listening for LoRa packets
  Serial.print(F("[SX1278] Starting to listen ... "));
  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // if needed, 'listen' mode can be disabled by calling
  // any of the following methods:
  //
  // radio.standby()
  // radio.sleep()
  // radio.transmit();
  // radio.receive();
  // radio.scanChannel();  



if (state == RADIOLIB_LORA_DETECTED) {
    // Kanal ist belegt!
    Serial.println(F("Besetzt! Senden verschoben."));
  } else {
    // Kanal ist frei
    Serial.println(F("Frei! Sende jetzt..."));
    radio.startTransmit("Hello World!");
  }

  // you can also transmit byte array up to 255 bytes long
  /*
    byte byteArr[] = {0x01, 0x23, 0x45, 0x67,
                      0x89, 0xAB, 0xCD, 0xEF};
    transmissionState = radio.startTransmit(byteArr, 8);
  */

  
}

void loop() {
  // put your main code here, to run repeatedly:




  if(receivedFlag) {
    // reset flag
    receivedFlag = false;

    // you can read received data as an Arduino String
    String str;
    int state = radio.readData(str);

    // you can also read received data as byte array
    /*
      byte byteArr[8];
      int numBytes = radio.getPacketLength();
      int state = radio.readData(byteArr, numBytes);
    */

    if (state == RADIOLIB_ERR_NONE) {
      // packet was successfully received
      Serial.println(F("[SX1278] Received packet!"));

      // print data of the packet
      Serial.print(F("[SX1278] Data:\t\t"));
      Serial.println(str);

      // print RSSI (Received Signal Strength Indicator)
      Serial.print(F("[SX1278] RSSI:\t\t"));
      Serial.print(radio.getRSSI());
      Serial.println(F(" dBm"));

      // print SNR (Signal-to-Noise Ratio)
      Serial.print(F("[SX1278] SNR:\t\t"));
      Serial.print(radio.getSNR());
      Serial.println(F(" dB"));

      // print frequency error
      Serial.print(F("[SX1278] Frequency error:\t"));
      Serial.print(radio.getFrequencyError());
      Serial.println(F(" Hz"));

    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      // packet was received, but is malformed
      Serial.println(F("[SX1278] CRC error!"));

    } else {
      // some other error occurred
      Serial.print(F("[SX1278] Failed, code "));
      Serial.println(state);

    }

  radio.startReceive();

  }
}

