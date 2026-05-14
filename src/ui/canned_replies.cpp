#include "canned_replies.h"

static const char *kDefaults[CANNED_REPLY_COUNT] = {
    "OK",
    "Yes",
    "No",
    "On my way",
    "Running late",
    "Phone me",
    "Talk Later",
    "Stand by",
};

const char *cannedReplyDefault(uint8_t idx) {
    if (idx >= CANNED_REPLY_COUNT) return "";
    return kDefaults[idx];
}

const char *cannedReplyGet(uint8_t idx) {
    if (idx >= CANNED_REPLY_COUNT) return "";
    return kDefaults[idx];
}
