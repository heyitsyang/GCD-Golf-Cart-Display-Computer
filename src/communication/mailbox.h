#ifndef MAILBOX_H
#define MAILBOX_H

#include <Arduino.h>

// Mailbox-available glyph state and sensor pairing.
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

// Reads the paired id from NVS. Call from loadPreferences() only.
void     mailboxLoad(void);

// Parser task. isPairOffer = the frame carried mode=PAIR.
void     mailboxOnFrame(uint32_t mbxId, bool present, bool isPairOffer);

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

// GUI task. Renders the pair-button label, e.g. "PAIR MBX a1b2c3d4".
void     mailboxStatusText(char *dst, size_t n);

#endif // MAILBOX_H
