#ifndef MAILBOX_H
#define MAILBOX_H

#include <Arduino.h>

// Mailbox-available glyph state, sensor pairing, and sensor health.
//
// A mailbox sensor broadcasts HoT type-03 frames. State frames carry
// PRESENT/ABSENT; a frame with mode=PAIR is an offer to be claimed, sent when
// the user presses the pair button on the sensor. The sensor is transmit-only:
// there is no handshake, so pairing is one-directional consent — the GCD
// accepts an offer locally and the sensor is never told.
//
// Threading: every shared word has exactly one writer (see mailbox.cpp), so no
// mutex is needed. mailboxLoad() runs before the queues exist and must not use
// queuePreferenceWrite(); mailboxGlyphOn() is polled by an EEZ getter before the
// mutexes exist and must not take any lock.

// Health of the paired sensor, driving the colour of lbl_mailbox_id_str.
enum MailboxHealth {
    MBX_HEALTH_NEUTRAL = 0,  // white  — unpaired, an offer is pending, or no contact yet
    MBX_HEALTH_OK      = 1,  // green  — heard, nothing missed
    MBX_HEALTH_MISSED  = 2,  // yellow — heard, but N frames were missed
    MBX_HEALTH_SILENT  = 3   // red    — paired and nothing heard for 24 h of listening
};

// Reads the paired id from NVS. Call from loadPreferences() only.
void     mailboxLoad(void);

// Parser task. isPairOffer = the frame carried mode=PAIR. seqValid is false when
// the frame had no parseable seq field, in which case gap counting is skipped.
void     mailboxOnFrame(uint32_t mbxId, bool present, bool isPairOffer,
                        uint8_t seq, bool seqValid);

// GUI task / EEZ getter. True when the glyph should be lit.
bool     mailboxGlyphOn(void);

// GUI task. Extinguishes the glyph until the next state change.
void     mailboxDismiss(void);

uint32_t mailboxGetPairedId(void);

// GUI task. The pending offer id, or 0 if there is none or it has gone stale.
uint32_t mailboxFreshOfferId(void);

// GUI task. Claims the fresh offer. Returns true if the pairing changed.
bool     mailboxAcceptOffer(void);

// GUI task. Unpairs; the feature goes dormant until something is paired again.
void     mailboxForget(void);

// GUI task. Text for lbl_mailbox_id_str, served through get_var_mailbox_id_str().
// Points at a static buffer owned by mailbox.cpp.
const char* mailboxIdText(void);

// GUI task. Which colour lbl_mailbox_id_str should be drawn in.
uint8_t  mailboxHealth(void);

#endif // MAILBOX_H
