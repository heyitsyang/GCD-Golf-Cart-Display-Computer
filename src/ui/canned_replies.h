#ifndef CANNED_REPLIES_H
#define CANNED_REPLIES_H

#include <Arduino.h>
#include "config.h"

// Return the default text for slot idx (0..CANNED_REPLY_COUNT-1).
const char *cannedReplyDefault(uint8_t idx);

// Return text for slot idx. Empty string if idx out of range.
const char *cannedReplyGet(uint8_t idx);

#endif // CANNED_REPLIES_H
