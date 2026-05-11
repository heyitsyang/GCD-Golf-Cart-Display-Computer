#include "canned_replies.h"

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

const char *cannedReplyDefault(uint8_t idx) {
    if (idx >= CANNED_REPLY_COUNT) return "";
    return kDefaults[idx];
}

const char *cannedReplyGet(uint8_t idx) {
    if (idx >= CANNED_REPLY_COUNT) return "";
    return kDefaults[idx];
}
