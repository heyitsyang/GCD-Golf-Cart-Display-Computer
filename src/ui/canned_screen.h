#ifndef CANNED_SCREEN_H
#define CANNED_SCREEN_H

#include <Arduino.h>
#include "communication/chat_buffer.h"
#include "meshtastic/channel.pb.h"

// One-time wiring. Call from setup() AFTER ui_init().
void cannedScreenInit();

// Set hub state to "reply to existing message". channel and dest set
// the TX target; srcMsg is the original message shown in the context strip.
void cannedScreenSetReplyMode(uint8_t channel, uint32_t dest, const chatMessage_t *srcMsg);

// Set hub state to "new compose". The hub shows its in-hub channel
// selector; dest defaults to BROADCAST_ADDR.
void cannedScreenSetNewMode();

// GUI-task drain hook: apply pending mode/context updates.
void cannedScreenPump();

// Called from meshtastic_admin admin_portnum_callback when a get_channel_response arrives.
void cannedScreenOnChannelResponse(const meshtastic_Channel *ch);

// Called from handleGcmRebooted() to clear stale channel names before re-fetch.
void cannedScreenResetChannelNames();

#endif // CANNED_SCREEN_H
