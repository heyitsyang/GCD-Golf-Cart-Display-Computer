#include "chat_buffer.h"
#include "config.h"
#include "globals.h"
#include <Meshtastic.h>
#include <string.h>
#include <time.h>

static chatMessage_t s_buffer[CHAT_BUFFER_SIZE];
static size_t   s_head = 0;        // next write index
static size_t   s_count = 0;       // populated slots, saturates at CHAT_BUFFER_SIZE
static uint32_t s_nextId = 1;      // monotonic id source

// Side-array: authoritative source for unread DM count.
// Unread DMs here survive ring-buffer eviction so the home-screen glyph
// stays accurate regardless of how many other messages arrive.
// Capped at DM_SLOT_MAX; oldest is evicted when a 5th DM arrives.
static chatMessage_t s_dmSlots[DM_SLOT_MAX];
static size_t        s_dmCount = 0;

static bool dmPredicate(const chatMessage_t *m) {
    return (m->to == my_node_num) ||
           (m->outgoing && m->to != BROADCAST_ADDR);
}

// isHot is passed explicitly rather than read from m->text: for a stored
// ring-buffer entry, text[] has already been run through chatAbbreviate()
// and no longer starts with '|', so hot-ness must be captured at append
// time (see chatBufferAppend) before that rewrite happens.
static bool matchesFilter(const chatMessage_t *m, bool isHot, uint8_t filter) {
    switch (filter) {
        case CHAT_FILTER_DM:    return dmPredicate(m);
        case CHAT_FILTER_ALL:   return !isHot;
        case CHAT_FILTER_CH0:   return !isHot && m->channel == 0;
        case CHAT_FILTER_CH1:   return !isHot && m->channel == 1;
        case CHAT_FILTER_CH2:   return !isHot && m->channel == 2;
        case CHAT_FILTER_DEBUG: return true;
        default:                return true;
    }
}

void chatAbbreviate(const char *src, char *dst, size_t dstSize) {
    if (!dst || dstSize == 0) return;
    dst[0] = '\0';
    if (!src) return;

    // HoT packets: |#01#..., |#02#..., |<other>
    // Type digits live at text[2..3] (matches parseHotPacketType()).
    if (src[0] == '|') {
        if (src[1] == '#' && src[2] == '0' && src[3] == '1') {
            snprintf(dst, dstSize, "[WX_BCAST]");
        } else if (src[1] == '#' && src[2] == '0' && src[3] == '2') {
            snprintf(dst, dstSize, "[ENT_BCAST]");
        } else {
            snprintf(dst, dstSize, "[HoT_PACKET]");
        }
        return;
    }

    // GC packets: ~#<NN>#GC#<PAYLOAD>#
    if (src[0] == '~' && src[1] == '#') {
        const char *p = src + 2;
        const char *nn_end = strchr(p, '#');
        if (nn_end) {
            const char *gc_start = nn_end + 1;
            const char *gc_end = strchr(gc_start, '#');
            if (gc_end && (gc_end - gc_start) == 2 &&
                gc_start[0] == 'G' && gc_start[1] == 'C') {
                const char *payload_start = gc_end + 1;
                const char *payload_end = strchr(payload_start, '#');
                if (payload_end) {
                    size_t plen = (size_t)(payload_end - payload_start);
                    if (plen == 0) {
                        snprintf(dst, dstSize, "<GC>");
                    } else {
                        // dstSize must hold '<' + payload + '>' + '\0'
                        size_t cap = (dstSize >= 3) ? dstSize - 3 : 0;
                        if (plen > cap) plen = cap;
                        dst[0] = '<';
                        memcpy(dst + 1, payload_start, plen);
                        dst[1 + plen] = '>';
                        dst[2 + plen] = '\0';
                    }
                    return;
                }
            }
        }
        // Malformed ~# — fall through to verbatim
    }

    // Default: copy verbatim, truncated
    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

void chatBufferAppend(const chatMessage_t *msg) {
    if (!msg) return;
    bool isHot = (msg->text[0] == '|');
    if (!msg->outgoing && !dmPredicate(msg) && !matchesFilter(msg, isHot, (uint8_t)mesh_filter)) return;

    if (xSemaphoreTake(chatBufferMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println("chatBufferAppend: mutex timeout, dropping");
        return;
    }

    bool overwrote = (s_count == CHAT_BUFFER_SIZE);
    uint32_t evicted_id = overwrote ? s_buffer[s_head].id : 0;
    size_t   evicted_slot = s_head;

    chatMessage_t *slot = &s_buffer[s_head];
    slot->id        = s_nextId++;
    slot->from      = msg->from;
    slot->to        = msg->to;
    slot->channel   = msg->channel;
    slot->timestamp = (msg->timestamp != 0) ? msg->timestamp : (uint32_t)time(NULL);
    slot->outgoing  = msg->outgoing;
    slot->read      = false;
    slot->isHot     = isHot;
    chatAbbreviate(msg->text, slot->text, sizeof(slot->text));

    s_head = (s_head + 1) % CHAT_BUFFER_SIZE;
    if (s_count < CHAT_BUFFER_SIZE) s_count++;

    // Mirror incoming DMs into the side-array so the unread count survives ring eviction.
    if (!slot->outgoing && slot->to == my_node_num) {
        if (s_dmCount == DM_SLOT_MAX) {
            memmove(&s_dmSlots[0], &s_dmSlots[1], (DM_SLOT_MAX - 1) * sizeof(chatMessage_t));
            s_dmSlots[DM_SLOT_MAX - 1] = *slot;
        } else {
            s_dmSlots[s_dmCount++] = *slot;
        }
    }

    xSemaphoreGive(chatBufferMutex);

#if DEBUG_HEAP
    if (overwrote) {
        Serial.printf("[HEAP] ring overwrite (id=%u evicted slot %u): free=%u\n",
                      (unsigned)slot->id, (unsigned)evicted_slot,
                      (unsigned)ESP.getFreeHeap());
        (void)evicted_id;
    }
#else
    (void)overwrote;
    (void)evicted_id;
    (void)evicted_slot;
#endif
}

size_t chatBufferSnapshot(uint8_t filter, chatMessage_t *out, size_t maxN) {
    if (!out || maxN == 0) return 0;
    if (xSemaphoreTake(chatBufferMutex, pdMS_TO_TICKS(100)) != pdTRUE) return 0;

    // Iterate oldest-to-newest. If buffer not full, that's [0..count-1].
    // If full, it's [head..head+CHAT_BUFFER_SIZE-1] (wrapping).
    size_t start = (s_count == CHAT_BUFFER_SIZE) ? s_head : 0;

    // First pass: count matches so we can keep the newest maxN.
    size_t matches = 0;
    for (size_t i = 0; i < s_count; i++) {
        const chatMessage_t *m = &s_buffer[(start + i) % CHAT_BUFFER_SIZE];
        if (matchesFilter(m, m->isHot, filter)) matches++;
    }

    size_t skip = (matches > maxN) ? (matches - maxN) : 0;
    size_t out_idx = 0;
    size_t seen = 0;
    for (size_t i = 0; i < s_count && out_idx < maxN; i++) {
        const chatMessage_t *m = &s_buffer[(start + i) % CHAT_BUFFER_SIZE];
        if (!matchesFilter(m, m->isHot, filter)) continue;
        if (seen++ < skip) continue;
        out[out_idx++] = *m;
    }

    xSemaphoreGive(chatBufferMutex);
    return out_idx;
}

bool chatBufferGetById(uint32_t id, chatMessage_t *out) {
    if (!out) return false;
    bool found = false;
    if (xSemaphoreTake(chatBufferMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
    for (size_t i = 0; i < s_count; i++) {
        if (s_buffer[i].id == id) {
            *out = s_buffer[i];
            found = true;
            break;
        }
    }
    xSemaphoreGive(chatBufferMutex);
    return found;
}

void chatBufferMarkRead(uint32_t id) {
    if (!chatBufferMutex || xSemaphoreTake(chatBufferMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    for (size_t i = 0; i < s_count; i++) {
        if (s_buffer[i].id == id) {
            s_buffer[i].read = true;
            break;
        }
    }
    for (size_t i = 0; i < s_dmCount; i++) {
        if (s_dmSlots[i].id == id) {
            memmove(&s_dmSlots[i], &s_dmSlots[i + 1], (s_dmCount - i - 1) * sizeof(chatMessage_t));
            s_dmCount--;
            break;
        }
    }
    xSemaphoreGive(chatBufferMutex);
}

size_t chatBufferUnreadDmCount(void) {
    if (!chatBufferMutex || xSemaphoreTake(chatBufferMutex, pdMS_TO_TICKS(100)) != pdTRUE) return 0;
    size_t n = s_dmCount;
    xSemaphoreGive(chatBufferMutex);
    return n;
}

size_t chatBufferSnapshotUnreadDms(chatMessage_t *out, size_t maxN) {
    if (!out || maxN == 0) return 0;
    if (!chatBufferMutex || xSemaphoreTake(chatBufferMutex, pdMS_TO_TICKS(100)) != pdTRUE) return 0;
    size_t n = (s_dmCount < maxN) ? s_dmCount : maxN;
    memcpy(out, &s_dmSlots[0], n * sizeof(chatMessage_t));
    xSemaphoreGive(chatBufferMutex);
    return n;
}
