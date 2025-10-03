#ifndef HOT_PACKET_PARSER_H
#define HOT_PACKET_PARSER_H

#include <Arduino.h>

bool isHotPacket(const char* text);
int parseHotPacketType(const char* text);
void processHotPacket(const char* text);
int parseWeatherData(char* input, const String& timestamp);
void parseVenueEventData(const char* input);

#endif // HOT_PACKET_PARSER_H