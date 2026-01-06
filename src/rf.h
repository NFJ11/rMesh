#ifndef RF_H
#define RF_H
#include <RadioLib.h>

extern bool radioFlag;
extern SX1278 radio;

void initRadio();
bool transmit(uint8_t*, size_t);
void addSourceCall(uint8_t* data, uint8_t &len);


#endif
