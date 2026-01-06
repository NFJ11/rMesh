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


struct Frame {
    uint32_t transmitMillis;
    uint8_t frameType = 0x00;
    String srcCall = "";
    uint8_t srcHeader;
	String dstCall = "";
    uint8_t dstHeader;
    std::vector<String> viaCall;
    std::vector<uint8_t> viaHeader;
    uint8_t retry = 0;
};

enum FrameType {
    ANNOUNCE,  
    ANNOUNCE_REPLY,
    TUNE
};

enum HeaderType {
    SRC_CALL = 0x00,  
    DST_CALL = 0x10
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



#endif
