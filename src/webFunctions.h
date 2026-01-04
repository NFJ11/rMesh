#define WS_MAX_QUEUED_MESSAGES 1

#ifndef WEBFUNCTIONS_H
#define WEBFUNCTIONS_H

#include <Arduino.h>
#include "ESPAsyncWebServer.h"


extern AsyncWebSocket ws;

void startWebServer();
String webProcessor(const String& var);

#endif
