#ifndef CANNED_REPLIES_H
#define CANNED_REPLIES_H

#include <Arduino.h>
#include "config.h"

// Read default text for slot idx (0..CANNED_REPLY_COUNT-1).
// Used when an NVS key is absent on first boot.
const char *cannedReplyDefault(uint8_t idx);

// Load all 8 slots from NVS into RAM. Falls back to defaults if a key is
// absent. Call from loadPreferences() after prefs.begin().
void cannedRepliesLoad();

// Return current text for slot idx. Empty string if idx out of range.
const char *cannedReplyGet(uint8_t idx);

// Update slot idx in RAM and queue an NVS write. Truncates to
// CANNED_REPLY_MAXLEN. No-op if idx out of range.
void cannedReplySet(uint8_t idx, const char *text);

// Keyboard bridge done-callback for canned-edit mode. userdata is
// (void*)(uintptr_t)slotIdx.
void saveCannedSlot(const char *text, void *userdata);

#endif // CANNED_REPLIES_H
