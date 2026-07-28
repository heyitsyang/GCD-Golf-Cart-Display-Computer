#include "mailbox.h"
#include "config.h"
#include "globals.h"
#include "hardware/display.h"
#include "storage/preferences_manager.h"

// An offer stays claimable this long after the frame that carried it.
// The sensor sends one PAIR frame per button press, so this is the driver's
// entire budget to see the offer and accept it. Pairing is done with the cart
// beside the mailbox; if the frame is lost, the recovery is another press.
#define MBX_OFFER_TTL_MS 45000UL

// ---------------------------------------------------------------------------
// State. Exactly one writer per word — that is what makes this lock-free.
//
//   s_pairedMbxId  : GUI task only (accept / forget / load-at-boot)
//   s_mailPresent  : parser task only
//   s_stateEpoch   : parser task only, bumped ONLY on a PRESENT/ABSENT edge
//   s_dismissEpoch : GUI task only
//   s_offerId      : parser task only
//   s_offerMs      : parser task only
//
// Aligned 32-bit and single-byte loads/stores are atomic on the ESP32's LX6
// cores, which share DRAM with hardware coherency, so no value can tear.
// volatile keeps the GUI task's poll from being hoisted out of its loop.
// ---------------------------------------------------------------------------
static uint32_t          s_pairedMbxId  = 0;   // 0 = nothing paired
static volatile bool     s_mailPresent  = false;
static volatile uint32_t s_stateEpoch   = 0;
static volatile uint32_t s_dismissEpoch = 0;
static volatile uint32_t s_offerId      = 0;
static volatile uint32_t s_offerMs      = 0;

// Clears glyph state so a freshly paired sensor starts dark. Without this,
// re-pairing while the previous mailbox was PRESENT would leave the glyph lit
// until the new sensor's next frame — up to an hour.
static void resetGlyphState() {
    s_mailPresent  = false;
    s_dismissEpoch = s_stateEpoch;
}

void mailboxLoad(void) {
    // prefs.getInt() directly: this runs from loadPreferences(), long before
    // eepromWriteQueue exists, so queuePreferenceWrite() is not available.
    s_pairedMbxId = (uint32_t)prefs.getInt("mbx_id", 0);
}

void mailboxOnFrame(uint32_t mbxId, bool present, bool isPairOffer) {
    // A PAIR frame's remaining fields are not trustworthy by contract, so it
    // only ever registers an offer — it never drives the glyph.
    if (isPairOffer) {
        s_offerId = mbxId;
        s_offerMs = millis();
        // Unconditional, like ESP-NOW pairing: this is rare, user-initiated, and
        // the only feedback available when a pairing does not appear on screen.
        Serial.printf("MBX pair offer: %08x (claimable %lus)\n",
                      mbxId, (unsigned long)(MBX_OFFER_TTL_MS / 1000));
        return;
    }

    if (s_pairedMbxId == 0 || mbxId != s_pairedMbxId) {
#if DEBUG_GCM_MESSAGES
        Serial.printf("MBX state %08x ignored (paired=%08x)\n", mbxId, s_pairedMbxId);
#endif
        return;
    }

    // Epoch bumps only on an edge. That is what makes the sensor's hourly
    // re-assertion a no-op against a user dismiss: no edge, no bump, so
    // s_dismissEpoch still matches and the glyph stays off.
    if (present != s_mailPresent) {
        s_mailPresent = present;
        s_stateEpoch++;
        Serial.printf("MBX %08x -> %s\n", mbxId, present ? "PRESENT" : "ABSENT");
        if (present) tone_alert();
    }
}

bool mailboxGlyphOn(void) {
    return s_mailPresent && (s_dismissEpoch != s_stateEpoch);
}

void mailboxDismiss(void) {
    s_dismissEpoch = s_stateEpoch;
}

uint32_t mailboxGetPairedId(void) {
    return s_pairedMbxId;
}

uint32_t mailboxFreshOfferId(void) {
    uint32_t id = s_offerId;
    if (id == 0) return 0;
    // Unsigned subtraction handles the ~49-day millis() wrap correctly.
    if ((millis() - s_offerMs) >= MBX_OFFER_TTL_MS) return 0;
    return id;
}

bool mailboxAcceptOffer(void) {
    uint32_t id = mailboxFreshOfferId();
    if (id == 0 || id == s_pairedMbxId) return false;
    s_pairedMbxId = id;
    resetGlyphState();
    Serial.printf("MBX paired: %08x\n", id);
    return true;
}

void mailboxForget(void) {
    if (s_pairedMbxId == 0) return;
    Serial.printf("MBX unpaired (was %08x)\n", s_pairedMbxId);
    s_pairedMbxId = 0;
    resetGlyphState();
}

void mailboxStatusText(char *dst, size_t n) {
    if (!dst || n == 0) return;
    uint32_t offer = mailboxFreshOfferId();

    if (offer != 0 && offer != s_pairedMbxId) {
        snprintf(dst, n, "PAIR MBX %08x", offer);        // claimable
    } else if (s_pairedMbxId == 0) {
        snprintf(dst, n, "NO MAILBOX");
    } else if (offer == s_pairedMbxId) {
        snprintf(dst, n, "MBX %08x RX", s_pairedMbxId);  // already ours, heard just now
    } else {
        snprintf(dst, n, "MBX %08x", s_pairedMbxId);
    }
}
