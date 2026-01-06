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
    uint32_t transmitMillis;
    uint8_t frameType = 0x00;
    //String srcCall = "";
    //uint8_t srcHeader;
	
    CallsignWithHeader srcCall;
    CallsignWithHeader dstCall;
    std::vector<CallsignWithHeader> viaCall;

    //String dstCall = "";
    //uint8_t dstHeader;
    
    //std::vector<String> viaCall;
    //std::vector<uint8_t> viaHeader;
    uint8_t retry = 0;
    char message[256];
    uint8_t messageLength = 0;
    uint32_t id = 0;
};



enum FrameType {
    ANNOUNCE,  
    ANNOUNCE_REPLY,
    TUNE,
    TEXT_MESSAGE
};

enum HeaderType {
    SRC_CALL = 0x00,  
    DST_CALL = 0x10,
    MESSAGE = 0x20,
    VIA_REPEAT = 0x30,
    VIA_NO_REPEAT = 0x40
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



#endif
