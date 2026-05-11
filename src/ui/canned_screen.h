#ifndef CANNED_SCREEN_H
#define CANNED_SCREEN_H

#include <Arduino.h>

// One-time wiring. Call from setup() AFTER ui_init().
void cannedScreenInit();

// Set hub state to "reply to existing message". Channel and dest
// come from the source message; contextText is shown in the strip.
void cannedScreenSetReplyMode(uint8_t channel, uint32_t dest, const char *contextText);

// Set hub state to "new compose". The hub shows its in-hub channel
// selector; dest defaults to BROADCAST_ADDR.
void cannedScreenSetNewMode();

// GUI-task drain hook: apply pending mode/context updates.
void cannedScreenPump();

#endif // CANNED_SCREEN_H
