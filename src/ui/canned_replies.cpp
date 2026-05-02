#include "canned_replies.h"
#include "globals.h"
#include "storage/preferences_manager.h"

static const char *kDefaults[CANNED_REPLY_COUNT] = {
    "OK",
    "On my way",
    "Yes",
    "No",
    "Stand by",
    "Heading home",
    "Phone me",
    "Running late",
};

static String s_replies[CANNED_REPLY_COUNT];

static void nvsKeyForSlot(uint8_t idx, char *buf, size_t bufSize) {
    snprintf(buf, bufSize, "reply%u", (unsigned)idx);
}

const char *cannedReplyDefault(uint8_t idx) {
    if (idx >= CANNED_REPLY_COUNT) return "";
    return kDefaults[idx];
}

void cannedRepliesLoad() {
    char key[8];
    for (uint8_t i = 0; i < CANNED_REPLY_COUNT; i++) {
        nvsKeyForSlot(i, key, sizeof(key));
        s_replies[i] = prefs.getString(key, kDefaults[i]);
        if (s_replies[i].length() > CANNED_REPLY_MAXLEN) {
            s_replies[i] = s_replies[i].substring(0, CANNED_REPLY_MAXLEN);
        }
    }
}

const char *cannedReplyGet(uint8_t idx) {
    if (idx >= CANNED_REPLY_COUNT) return "";
    return s_replies[idx].c_str();
}

void cannedReplySet(uint8_t idx, const char *text) {
    if (idx >= CANNED_REPLY_COUNT || !text) return;

    String v = text;
    if (v.length() > CANNED_REPLY_MAXLEN) {
        v = v.substring(0, CANNED_REPLY_MAXLEN);
    }
    s_replies[idx] = v;

    char key[8];
    nvsKeyForSlot(idx, key, sizeof(key));
    queuePreferenceWrite(key, v);
}

void saveCannedSlot(const char *text, void *userdata) {
    uint8_t idx = (uint8_t)(uintptr_t)userdata;
    cannedReplySet(idx, text);
}
