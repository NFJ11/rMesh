#include "rf.h"
#include "main.h"
#include <RadioLib.h>
#include "settings.h"
#include <LittleFS.h>

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

void monitorFrame(Frame &f) {
    //Frame über Websocket senden
    JsonDocument doc;
    for (uint16_t i = 0; i < f.rawDataLength; i++) {
        doc["monitor"]["rawData"][i] = f.rawData[i];
    }
    for (uint16_t i = 0; i < f.messageLength; i++) {
        doc["monitor"]["message"][i] = static_cast<uint8_t>(f.message[i]);
    }    
    doc["monitor"]["tx"] = f.tx;
    doc["monitor"]["rssi"] = f.rssi;
    doc["monitor"]["snr"] = f.snr;
    doc["monitor"]["frequencyError"] = f.frqError;
    doc["monitor"]["time"] = f.time;
    doc["monitor"]["srcCall"] = f.srcCall.call;
    doc["monitor"]["srcCallHeader"] = f.srcCall.header;
    doc["monitor"]["dstCall"] = f.dstCall.call;
    doc["monitor"]["dstCallHeader"] = f.dstCall.header;
    doc["monitor"]["viaCall"] = f.viaCall.call;
    doc["monitor"]["viaCallHeader"] = f.viaCall.header;
    doc["monitor"]["nodeCall"] = f.nodeCall.call;
    doc["monitor"]["nodeCallHeader"] = f.nodeCall.header;
    doc["monitor"]["frameType"] = f.frameType;
    doc["monitor"]["id"] = f.id;
    doc["monitor"]["initRetry"] = f.initRetry;
    doc["monitor"]["retry"] = f.retry;
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
    if (f.srcCall.call.length() > 0) {
        f.rawData[f.rawDataLength] = SRC_CALL << 4 | (0x0F & f.srcCall.call.length());  //Header Absender
        f.rawDataLength ++;
        memcpy(&f.rawData[f.rawDataLength], &f.srcCall.call[0], f.srcCall.call.length()); //Payload
        f.rawDataLength += f.srcCall.call.length();
    }
    //Node hinzu
    f.rawData[f.rawDataLength] = NODE_CALL << 4 | (0x0F & strlen(settings.mycall));  //Header Absender
    f.rawDataLength ++;
    memcpy(&f.rawData[f.rawDataLength], &settings.mycall[0], strlen(settings.mycall)); //Payload
    f.rawDataLength += strlen(settings.mycall);
    //VIA hinzu
    if (f.viaCall.call.length() > 0) {
        f.rawData[f.rawDataLength] = VIA_CALL << 4 | (0x0F & f.viaCall.call.length());  //Header Absender
        f.rawDataLength ++;
        memcpy(&f.rawData[f.rawDataLength], &f.viaCall.call[0], f.viaCall.call.length()); //Payload
        f.rawDataLength += f.viaCall.call.length();
    }
    //Empfänger hinzu
    if (f.dstCall.call.length() > 0) {
        f.rawData[f.rawDataLength] = DST_CALL << 4 | (0x0F & f.dstCall.call.length());  //Header Absender
        f.rawDataLength ++;
        memcpy(&f.rawData[f.rawDataLength], &f.dstCall.call[0], f.dstCall.call.length()); //Payload
        f.rawDataLength += f.dstCall.call.length();
    }
    //Message hinzu (muss ganz hinten sein)
    if ((f.frameType == TEXT_MESSAGE) || (f.frameType == MESSAGE_ACK)) {
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
    f.nodeCall.call = String(settings.mycall);
    f.nodeCall.header = NODE_CALL;
    f.dstCall.header = DST_CALL;
    f.viaCall.header = VIA_CALL;
    f.rssi = 0;
    f.snr = 0;
    f.frqError = 0;

    // Prüfen, ob der Kanal frei ist (Channel Activity Detection - CAD)
    if (radio.scanChannel() == RADIOLIB_LORA_DETECTED) {
        //Kanal belegt
        return false;
    } else {
        //Senden
        transmittingFlag = true;
        statusTimer = 0;
        radio.startTransmit(f.rawData, f.rawDataLength);
        //Monitor
        monitorFrame(f);
    }
    return true;
}

void sendMessage(String dstCall, String text) {
    //Neuen Frame für alle Peers zusammenbauen
    bool suspendTX = false;
    uint8_t availableNodeCount = 0;
    Frame f;
    f.frameType = TEXT_MESSAGE;
    f.srcCall.call = String(settings.mycall);
    f.dstCall.call = dstCall;
    text.toCharArray((char*)f.message, 255);
    f.messageLength = text.length();
    f.id = millis();
    f.time = time(NULL);

    for (int i = 0; i < peerList.size(); i++) {
        if (peerList[i].available) {
            availableNodeCount ++;
            f.viaCall.call = peerList[i].call;
            f.retry = TX_RETRY;
            f.initRetry = TX_RETRY;
            f.suspendTX = suspendTX;
            //Ab dem 2. Frame -> SUSPEND
            if (suspendTX == false) { suspendTX = true; }
            //Frame in Sendebuffer
            txFrameBuffer.push_back(f);
        }
    } 

    //Wenn keine Peers, Frame ohne Ziel und Retry senden
    if (availableNodeCount == 0) {
        f.viaCall.call = "";
        f.retry = 1;
        f.initRetry = 1;
        f.suspendTX = suspendTX;
        //Frame in Sendebuffer
        txFrameBuffer.push_back(f);
    }

    //Message in Datei speichern
    JsonDocument doc;
    doc["message"]["text"] = text;
    doc["message"]["srcCall"] = String(settings.mycall);
    doc["message"]["dstCall"] = dstCall;
    doc["message"]["time"] = f.time;
    doc["message"]["id"] = f.id;
    doc["message"]["tx"] = true;
    String jsonOutput;
    serializeJson(doc, jsonOutput);
    File file = LittleFS.open("/messages.json", "a"); 
    if (file) {
        serializeJson(doc, file);
        file.println(); 
        file.close();
        limitFileLines("/messages.json", MAX_STORED_MESSAGES);
    }    

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


uint32_t getLoRaToA(uint8_t payloadSize, uint8_t SF, uint32_t BW, uint8_t CR, uint16_t nPreamble) {
    
    // Symbol-Dauer in Sekunden: T_sym = 2^SF / BW
    double tSym = pow(2, SF) / (double)BW;

    // Zeit für Präambel: (Anzahl + 4.25) * T_sym
    double tPreamble = (nPreamble + 4.25) * tSym;

    // Low Data Rate Optimization (muss bei SF11 & SF12 auf 125kHz BW an sein)
    bool lowDataRateOptimize = false;
    if (tSym > 0.016) { // Wenn Symbolzeit > 16ms
        lowDataRateOptimize = true;
    }

    // Payload Symbol Anzahl Berechnung
    double payloadSymbNb = 8 + max(ceil((8.0 * payloadSize - 4.0 * SF + 28 + 16) / 
                           (4.0 * (SF - 2.0 * lowDataRateOptimize))) * CR, 0.0);

    double tPayload = payloadSymbNb * tSym;
    double totalTimeSec = tPreamble + tPayload;

    // Rückgabe in Millisekunden (aufgerundet)
    return (uint32_t)(totalTimeSec * 1000.0 + 0.5);
}
