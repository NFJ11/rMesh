#ifndef RF_H
#define RF_H
#include <RadioLib.h>
#include <vector>

struct Peer {
    String call; 
    time_t lastRX;
    float rssi;
    float snr;
    float frqError;
    bool available;
};

struct CallsignWithHeader {
    String call = "";
    uint8_t header = 0;
};

struct Frame {
    uint32_t transmitMillis = 0;
    uint8_t frameType = 0x00;
    CallsignWithHeader srcCall;
    CallsignWithHeader dstCall;
    CallsignWithHeader nodeCall;
    CallsignWithHeader viaCall;
    //std::vector<CallsignWithHeader> viaCall;
    uint8_t retry = 1;
    uint8_t initRetry = 1;
    uint8_t message[256];
    uint16_t messageLength = 0;
    uint32_t id = 0;
    uint8_t rawData[256];
    uint16_t rawDataLength = 0;
    time_t time = 0;
    float rssi = 0;
    float snr = 0;
    float frqError = 0;
    bool tx = false;
    bool suspendTX = false;
};



enum FrameType {
    ANNOUNCE,  
    ANNOUNCE_REPLY,
    TUNE,
    TEXT_MESSAGE,
    MESSAGE_ACK,
    TEXT_MESSAGE_ROUTING
};

enum HeaderType {
    //Obere 4 Bits vom Header-Byte -> 0x00 bis 0x0F
    SRC_CALL,  
    DST_CALL,
    MESSAGE,
    NODE_CALL,
    VIA_CALL
};

extern bool transmittingFlag;
extern bool radioFlag;
extern SX1278 radio;
extern std::vector<Peer> peerList;
extern std::vector<Frame> txFrameBuffer;

void initRadio();
void addSourceCall(uint8_t* data, uint8_t &len);
void sendPeerList();
void addPeerList(Peer p);
void checkPeerList();
bool transmitFrame(Frame &f);
void sendMessage(String dstCall, String text);
void availablePeerList(String call, bool available);
void monitorFrame(Frame &f);
uint32_t getLoRaToA(uint8_t payloadSize, uint8_t SF, uint32_t BW, uint8_t CR, uint16_t nPreamble);
String messageJson(String text, String srcCall, String dstCall, String nodeCall, time_t time, uint32_t id, bool tx);

#endif
