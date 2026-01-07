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
    String call;
    uint8_t header;
};

struct Frame {
    uint32_t transmitMillis = 0;
    uint8_t frameType = 0x00;
    CallsignWithHeader srcCall;
    CallsignWithHeader dstCall;
    std::vector<CallsignWithHeader> viaCall;
    uint8_t retry = 1;
    uint8_t initRetry = 1;
    char message[256];
    uint16_t messageLength = 0;
    uint32_t id = 0;
    uint8_t rawData[256];
    uint16_t rawDataLength = 0;
    time_t time = 0;
    float rssi = 0;
    float snr = 0;
    float frqError = 0;
    bool tx = false;
};



enum FrameType {
    ANNOUNCE,  
    ANNOUNCE_REPLY,
    TUNE,
    TEXT_MESSAGE
};

enum HeaderType {
    //Obere 4 Bits vom Header-Byte -> 0x00 bis 0x0F
    SRC_CALL = 0x0,  
    DST_CALL = 0x1,
    MESSAGE = 0x2,
    VIA_REPEAT = 0x3,
    VIA_NO_REPEAT = 0x4
};

extern bool transmittingFlag;
extern bool radioFlag;
extern SX1278 radio;
extern std::vector<Peer> peerList;
extern std::vector<Frame> txFrameBuffer;

void initRadio();
bool transmitRAW(uint8_t*, size_t);
void addSourceCall(uint8_t* data, uint8_t &len);
void sendPeerList();
void addPeerList(Peer p);
void checkPeerList();
bool transmitFrame(Frame &f);
void sendMessage(String dstCall, String text);
void availablePeerList(String call, bool available);
void monitorFrame(Frame &f);



#endif
