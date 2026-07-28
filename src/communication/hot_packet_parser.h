#ifndef HOT_PACKET_PARSER_H
#define HOT_PACKET_PARSER_H

#include <Arduino.h>

bool isHotPacket(const char* text);
int parseHotPacketType(const char* text);

// '#'-delimited field helpers, shared with chat_buffer.cpp's abbreviation code.
// hotField() returns a pointer into `s` for field `idx` (0-based) with its
// length in *outLen, or NULL if the field is absent or empty. It never modifies
// the input, and tolerates a missing trailing '#'.
const char* hotField(const char* s, uint8_t idx, uint8_t* outLen);
bool hotParseHex8(const char* p, uint8_t len, uint32_t* out);
void processHotPacket(const char* text);
int parseWeatherData(char* input, const String& timestamp);
void parseVenueEventData(const char* input);

#endif // HOT_PACKET_PARSER_H