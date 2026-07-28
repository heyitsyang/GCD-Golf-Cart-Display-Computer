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

// Declare the sensor silent after this much *listening* time with nothing heard.
// Deliberately measured in awake time, not wall-clock: wall-clock cannot tell
// "the sensor is silent" from "the cart was parked and we were not listening",
// and a cart left for a few days would otherwise alarm on a healthy sensor.
//
// 24 h is roughly double the longest legitimate quiet period, since the sensor
// gates transmissions overnight (DDR04). In a NO_GCI install the GCD is powered
// continuously, so this elapses as intended; in a GCI install it deep-sleeps
// with the cart and the timer restarts each wake, so the warning rarely appears.
// That is graceful degradation, not a defect — a GCD that is not listening
// genuinely cannot distinguish a dead sensor from a parked cart.
#define MBX_SILENT_MS (24UL * 60UL * 60UL * 1000UL)

// ---------------------------------------------------------------------------
// State. Exactly one writer per word — that is what makes this lock-free.
//
//   s_pairedMbxId   : GUI task only (accept / forget / load-at-boot)
//   s_mailPresent   : parser task only
//   s_stateEpoch    : parser task only, bumped on any PRESENT frame and on edges
//   s_dismissEpoch  : GUI task only
//
// The epoch pair exists instead of a plain `bool dismissed` because that would
// have two writers — the parser clearing it and the GUI setting it — and the
// resulting read-modify-write race would occasionally drop a dismiss.
//   s_offerId       : parser task only
//   s_offerMs       : parser task only
//   s_lastSeq       : parser task only
//   s_seqValid      : parser task only
//   s_missedCount   : parser task only
//   s_lastHeardMs   : parser task only
//   s_heardThisWake : parser task only
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

// Health tracking. seq gaps are used instead of elapsed time because the sensor
// goes quiet overnight: a gated sensor does not advance seq, so the first frame
// after the gate is lastSeq+1 and no miss is recorded. Re-baselining on the
// first frame of each wake excludes cart-off time for the same reason.
static volatile uint8_t  s_lastSeq       = 0;
static volatile bool     s_seqValid      = false;
static volatile uint16_t s_missedCount   = 0;
static volatile uint32_t s_lastHeardMs   = 0;
static volatile bool     s_heardThisWake = false;
static uint32_t          s_bootMs        = 0;

// Clears glyph state so a freshly paired sensor starts dark. Without this,
// re-pairing while the previous mailbox was PRESENT would leave the glyph lit
// until the new sensor's next frame — up to an hour.
static void resetGlyphState() {
    s_mailPresent  = false;
    s_dismissEpoch = s_stateEpoch;
}

// Clears the health history. A newly paired sensor must not inherit the
// previous one's miss count or silence timer.
static void resetHealthState() {
    s_seqValid      = false;
    s_missedCount   = 0;
    s_heardThisWake = false;
    s_lastHeardMs   = millis();
}

void mailboxLoad(void) {
    // prefs.getInt() directly: this runs from loadPreferences(), long before
    // eepromWriteQueue exists, so queuePreferenceWrite() is not available.
    s_pairedMbxId = (uint32_t)prefs.getInt("mbx_id", 0);
    s_bootMs      = millis();
}

void mailboxOnFrame(uint32_t mbxId, bool present, bool isPairOffer,
                    uint8_t seq, bool seqValid) {
    bool isOurs = (s_pairedMbxId != 0 && mbxId == s_pairedMbxId);

    // An offer from a mailbox we are NOT paired to is a pairing opportunity.
    // An offer from the one we ARE paired to is just contact: accepting it would
    // be a no-op, so advertising it as actionable would mislead.
    if (isPairOffer && !isOurs) {
        s_offerId = mbxId;
        s_offerMs = millis();
        // Unconditional, like ESP-NOW pairing: this is rare, user-initiated, and
        // the only feedback available when a pairing does not appear on screen.
        Serial.printf("MBX pair offer: %08x (claimable %lus)\n",
                      mbxId, (unsigned long)(MBX_OFFER_TTL_MS / 1000));
        return;
    }

    if (!isOurs) {
#if DEBUG_GCM_MESSAGES
        Serial.printf("MBX state %08x ignored (paired=%08x)\n", mbxId, s_pairedMbxId);
#endif
        return;
    }

    // --- Contact, from any mode (NORM, DEV or PAIR) ---
    // Count frames missed between two we actually received. The first frame of
    // each wake only establishes a baseline, which is what keeps cart-off time
    // out of the count.
    //
    // seq is ONE BYTE, so this recovers (frames missed) mod 256, not the true
    // total: exactly 256 missed frames aliases to a gap of 0 and reads as a
    // perfect run. At the hourly re-assertion rate that is a ~10.7 day outage.
    // Accepted deliberately — the information is not in the packet, so no GCD
    // change can recover it. Read the count as a link-quality hint scoped to the
    // current session, not as an outage log. See design plan section 13.2.1.
    //
    // s_missedCount is uint16_t and accumulates across separate gap events
    // (saturating at 9999), so the displayed total can exceed 255 legitimately.
    // That ceiling is not the limitation; the per-event 8-bit aliasing is.
    if (seqValid) {
        if (s_seqValid) {
            uint8_t gap = (uint8_t)(seq - s_lastSeq - 1);
            if (gap != 0) {
                uint32_t total = (uint32_t)s_missedCount + gap;
                s_missedCount = (total > 9999) ? 9999 : (uint16_t)total;  // saturate
                Serial.printf("MBX %08x missed %u (total %u)\n", mbxId, gap, s_missedCount);
            }
        }
        s_lastSeq  = seq;
        s_seqValid = true;
    }
    s_lastHeardMs   = millis();
    s_heardThisWake = true;

    // A PAIR frame's state field is not trustworthy by contract, so it updates
    // health but must never drive the glyph.
    if (isPairOffer) return;

    // The epoch bumps on every PRESENT frame, edge or hourly re-assertion, which
    // makes a dismiss a *snooze*: it silences the glyph until the sensor next
    // reports mail still waiting, then the glyph returns. The mail has not gone
    // anywhere, so the reminder should not be permanently dismissible.
    //
    // The chime follows the GLYPH edge, not the mail-state edge. Those differ
    // only after a dismiss, and that is exactly the case that matters: the mail
    // state is already PRESENT, so a state-edge test sees no change and stays
    // silent, leaving a snoozed glyph to come back with no announcement.
    // Sampling before the mutation and comparing after covers every path with
    // one rule — "the glyph lit, so chime" — instead of enumerating them.
    bool wasOn = mailboxGlyphOn();

    if (present != s_mailPresent) {
        s_mailPresent = present;
        s_stateEpoch++;
        Serial.printf("MBX %08x -> %s\n", mbxId, present ? "PRESENT" : "ABSENT");
    } else if (present) {
        s_stateEpoch++;   // re-assertion: lapses any dismiss
    }

    // Still silent when an undismissed glyph is merely re-asserted hourly: it was
    // already lit, so this is not an edge and there is nothing new to announce.
    if (!wasOn && mailboxGlyphOn()) tone_alert();
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
    resetHealthState();
    s_offerId = 0;               // consumed
    Serial.printf("MBX paired: %08x\n", id);
    return true;
}

void mailboxForget(void) {
    if (s_pairedMbxId == 0) return;
    Serial.printf("MBX unpaired (was %08x)\n", s_pairedMbxId);
    s_pairedMbxId = 0;
    resetGlyphState();
    resetHealthState();
}

// True once nothing has been heard from the paired mailbox for MBX_SILENT_MS of
// listening. Measured from the last contact, or from boot if there has never
// been any — the already-dead-at-power-on case.
static bool mailboxSilent() {
    if (s_pairedMbxId == 0) return false;
    uint32_t since = millis() - (s_heardThisWake ? s_lastHeardMs : s_bootMs);
    return (since >= MBX_SILENT_MS);
}

uint8_t mailboxHealth(void) {
    // An offer is a call to action, not a health report — stay neutral so the
    // colour does not fight the OFFERED text.
    uint32_t offer = mailboxFreshOfferId();
    if (offer != 0 && offer != s_pairedMbxId) return MBX_HEALTH_NEUTRAL;

    if (s_pairedMbxId == 0)  return MBX_HEALTH_NEUTRAL;
    if (mailboxSilent())     return MBX_HEALTH_SILENT;
    if (!s_heardThisWake)    return MBX_HEALTH_NEUTRAL;   // awaiting first contact
    if (s_missedCount > 0)   return MBX_HEALTH_MISSED;
    return MBX_HEALTH_OK;
}

const char* mailboxIdText(void) {
    static char buf[24];
    uint32_t offer = mailboxFreshOfferId();

    if (offer != 0 && offer != s_pairedMbxId) {
        snprintf(buf, sizeof buf, "%08x OFFERED", offer);
    } else if (s_pairedMbxId == 0) {
        snprintf(buf, sizeof buf, "NO MAILBOX");
    } else if (mailboxSilent()) {
        snprintf(buf, sizeof buf, "%08x SILENT", s_pairedMbxId);
    } else if (s_heardThisWake && s_missedCount > 0) {
        // Parenthesised so the count reads as an annotation rather than as part
        // of the hex id, which is otherwise easy to misread at a glance.
        snprintf(buf, sizeof buf, "%08x (%u)", s_pairedMbxId, s_missedCount);
    } else {
        snprintf(buf, sizeof buf, "%08x", s_pairedMbxId);
    }
    return buf;
}
