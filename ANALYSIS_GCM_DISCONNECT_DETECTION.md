# GCM Disconnect Detection Analysis

## Executive Summary

**Question**: Can we reliably detect when GCM is disconnected before it reconnects?

**Short Answer**: **No, not reliably with current Meshtastic protocol over serial UART.**

**Current State**: We can detect **reconnection** perfectly (via `rebooted_tag`), but **disconnection** has no reliable signal.

---

## Current Mechanisms Analysis

### 1. Meshtastic Heartbeat (GCD → GCM)

**Location**: `lib/meshtastic-arduino_src/mt_protocol.cpp:789-792`

```cpp
// Sent every 60 seconds
if(now >= (last_heartbeat_at + HEARTBEAT_INTERVAL_MS)){
    mt_send_heartbeat();  // Tag 7 (heartbeat_tag)
    last_heartbeat_at = now;
}
```

**Purpose**: **REQUIRED** - GCM firmware will close the connection if no messages received for 15 minutes
**Direction**: ONE-WAY (GCD → GCM only)
**GCM Response**: **NONE** - The GCM does not acknowledge heartbeats
**Protocol Design**: Heartbeat is a "dummy" message with no response expected

**From mt_protocol.cpp comments (lines 24-26)**:
> "Otherwise the connection is closed, and packets will no longer be received
> We will send a ping every 60 seconds, which is what the web client does"
> Reference: [Meshtastic.js serialConnection.ts](https://github.com/meshtastic/js/blob/715e35d2374276a43ffa93c628e3710875d43907/src/adapters/serialConnection.ts#L160)

**From mesh.pb.h comments**:
> "A heartbeat message is sent to the node from the client to keep the connection alive.
> This is currently only needed to keep serial connections alive, but can be used by any PhoneAPI."

**Heartbeat structure**:
```c
typedef struct _meshtastic_Heartbeat {
    char dummy_field;  // Size: 0 bytes - literally empty!
} meshtastic_Heartbeat;
```

**Why it's needed**:
- GCM firmware expects at least one message every 15 minutes
- Without heartbeat, GCM will assume connection is dead and close it
- 60-second interval provides safety margin (15min / 15 = 4min, using 1min)
- This is a **firmware requirement**, not optional

**Conclusion**:
- Heartbeat is **MANDATORY** - DO NOT REMOVE
- It's a KEEPALIVE for the GCM, not a PING/PONG for disconnect detection
- No response expected or provided by GCM firmware

---

### 2. Packets Received From GCM

From test output analysis, during normal operation GCM sends:

| Tag | Name | Frequency | Trigger |
|-----|------|-----------|---------|
| 2 | packet_tag | Variable | Mesh network traffic (from other nodes) |
| 4 | node_info_tag | Frequent | Neighbor discovery/updates |
| 11 | queueStatus_tag | Periodic | Queue status updates |
| 15 | fileInfo_tag | Bursts | File transfer related |

**Key Observations**:
1. **No packets are guaranteed** - If mesh is quiet, no tag 2 packets
2. **Tag 4 (node_info) frequency depends on neighbors** - Could stop if no neighbors
3. **Tag 11 (queueStatus) appears periodic** - But frequency is unknown and may depend on queue activity
4. **No packet is a direct response to heartbeat** - Heartbeat is fire-and-forget

---

### 3. Serial UART Physical Layer

**Hardware**: ESP32 UART2 (HardwareSerial)
**Protocol**: 8N1 at 115200 baud (MT_DEV_BAUD_RATE)

**UART Limitations**:
- No hardware disconnect detection (unlike USB)
- No DTR/RTS flow control configured
- RX buffer will simply stop receiving data
- No error signaling when remote device powers off

**Potential UART-level detection**:
- Framing errors (if disconnected mid-transmission)
- RX buffer timeout (if we knew expected packet interval)

---

## Why Previous Attempts Failed

Based on analysis, likely failure modes:

### Scenario 1: Timeout Too Short
If timeout was set too aggressively (e.g., 30 seconds):
- Mesh network could be legitimately quiet for longer periods
- No guaranteed periodic packets from GCM
- False positives: declaring GCM disconnected when it's just idle

### Scenario 2: Timeout Too Long
If timeout was set conservatively (e.g., 120+ seconds):
- Defeats the purpose - user sees stale data for 2+ minutes
- By the time disconnect is detected, GCM may have already reconnected

### Scenario 3: Wrong Packet Choice
If monitoring a specific packet type (e.g., tag 11 queueStatus):
- Frequency may vary based on queue activity
- May not be sent if nothing is queued
- Unreliable timing

### Scenario 4: No Heartbeat Response
If expecting GCM to respond to heartbeat:
- Protocol doesn't specify a response
- GCM firmware doesn't send one
- Will never work without GCM firmware modification

---

## Test Output Pattern Analysis

### Normal Operation (GCM connected, stable)
```
FromRadio tag: 4  (every ~1-2 seconds based on grouping)
FromRadio tag: 2  (variable - depends on mesh traffic)
FromRadio tag: 11 (periodic - timing unknown)
ESP-NOW Telemetry from GCI (every ~1 second)
```

**Key Finding**: Even during "stable" operation, packet arrival is irregular. There are gaps in the FromRadio messages.

### During Reconnection
```
FromRadio tag: 8  ← DISCONNECT DETECTED HERE (GCM rebooted)
FromRadio tag: 3  ← GCM identity
FromRadio tag: 4  (37 times - all nodes)
FromRadio tag: 10 (8 times - all channels)
FromRadio tag: 5  (10 times - all configs)
FromRadio tag: 9  (13 times - all module configs)
FromRadio tag: 7  ← Handshake complete
FromRadio tag: 11 ← Normal operation resumes
FromRadio tag: 2  ← Mesh packets resume
```

**Key Finding**: `rebooted_tag` (tag 8) is a **perfect** reconnection indicator. It's sent immediately when GCM boots up.

---

## Constraints for Disconnect Detection

### Hard Constraints (Cannot Be Changed)
1. ✗ Meshtastic protocol does not define heartbeat response
2. ✗ GCM firmware cannot be modified (upstream Meshtastic)
3. ✗ UART has no hardware disconnect detection
4. ✗ Packet arrival timing is non-deterministic

### Soft Constraints (Can Work Around)
1. ✓ Can monitor any/all incoming packets
2. ✓ Can set reasonable timeout thresholds
3. ✓ Can distinguish between "disconnected" and "idle"
4. ✓ Can provide UI feedback about connection state

---

## Possible Approaches (Ranked by Reliability)

### Approach 1: Passive Monitoring with Conservative Timeout ⭐⭐⭐
**Reliability**: Medium
**Complexity**: Low
**False Positive Rate**: Low to Medium

**Method**:
- Track timestamp of last received packet (ANY tag)
- If no packet received in 90-120 seconds, assume disconnected
- Update UI to show "Connection uncertain" or similar

**Pros**:
- Simple to implement
- No protocol changes needed
- Low false positive rate

**Cons**:
- 90-120 second delay before detection
- Still possible false positives in very quiet mesh
- User sees stale node ID for extended period

**Implementation**:
```cpp
// In mt_protocol.cpp handle_packet()
static uint32_t last_packet_time = 0;

bool handle_packet(uint32_t now, size_t payload_len) {
    last_packet_time = now;  // Update on ANY packet
    // ... existing code ...
}

// In meshtastic_task.cpp
#define GCM_TIMEOUT_MS 120000  // 2 minutes
static bool gcm_connection_uncertain = false;

if ((millis() - last_packet_time) > GCM_TIMEOUT_MS) {
    if (!gcm_connection_uncertain) {
        gcm_connection_uncertain = true;
        Serial.println("*** GCM CONNECTION UNCERTAIN ***");
        // Optional: UI update, but don't clear node ID yet
    }
}
```

---

### Approach 2: Do Nothing, Rely on Reconnection Detection ⭐⭐⭐⭐⭐
**Reliability**: Perfect (for what it does)
**Complexity**: Zero (already implemented)
**False Positive Rate**: Zero

**Method**:
- Accept that disconnect detection is unreliable
- Use `rebooted_tag` to detect reconnection (already working)
- Clear UI state on reconnection, not on disconnect

**Pros**:
- Already fully implemented and tested
- Zero false positives
- Zero false negatives
- No additional complexity
- Matches actual protocol capabilities

**Cons**:
- Cannot detect disconnect proactively
- User doesn't know GCM is disconnected until reconnection
- Stale data may be displayed while disconnected

**Current Behavior** (already working):
```
1. GCM disconnected → No indication (no reliable signal)
2. GCM reconnects → rebooted_tag received
3. GCD responds → Clears node ID, sends wake notification
4. Handshake completes → Node ID restored, system operational
```

---

### Approach 3: Active Polling with Request/Response ⭐⭐
**Reliability**: High (if implemented)
**Complexity**: High
**False Positive Rate**: Very Low

**Method**:
- Periodically request node info from GCM
- Monitor if response is received
- If no response in 5-10 seconds, retry
- After 3 failed attempts, declare disconnected

**Pros**:
- Fast disconnect detection (15-30 seconds)
- Low false positive rate
- Active verification

**Cons**:
- Requires finding a GCM command that guarantees response
- Adds extra protocol traffic
- More complex state machine
- May still fail if GCM is busy/overloaded

**Potential Implementation**:
- Use `mt_request_node_report()` periodically
- Monitor if callback is invoked
- Problem: Callback may not be invoked if GCM is processing other things

---

### Approach 4: Enhanced UI Feedback (Hybrid) ⭐⭐⭐⭐
**Reliability**: High (for user experience)
**Complexity**: Low
**False Positive Rate**: N/A (no binary decision)

**Method**:
- Show "connection quality" based on packet recency
- Green: Packet received < 30 seconds ago
- Yellow: Packet received 30-90 seconds ago
- Gray: Packet received > 90 seconds ago
- Don't make binary connected/disconnected decision

**Pros**:
- Honest about connection uncertainty
- No false positives/negatives (just gradual indication)
- Helps user understand system state
- Simple to implement

**Cons**:
- Doesn't provide binary state for automation
- Still shows stale data while disconnected
- Doesn't address the root issue

---

## Recommendations

### Primary Recommendation: **Approach 2 (Do Nothing)** ⭐⭐⭐⭐⭐

**Rationale**:
1. **Current system is working correctly** - Reconnection detection via `rebooted_tag` is perfect
2. **Protocol limitations are fundamental** - Meshtastic protocol doesn't support reliable disconnect detection over serial
3. **Risk of false positives** - Any timeout-based approach will have edge cases
4. **Complexity vs. benefit** - Adding disconnect detection adds complexity for minimal practical benefit

**What we have now**:
- ✅ Crash fixed (NULL pointer checks)
- ✅ Reconnection detected immediately (rebooted_tag)
- ✅ State properly reset on reconnection
- ✅ Full handshake completes automatically
- ✅ System is stable and reliable

**What disconnect detection would add**:
- Knowing GCM is disconnected ~60-120 seconds sooner
- Clearing stale node ID slightly earlier
- More complex code with potential for bugs

### Secondary Recommendation: **Approach 4 (Enhanced UI)** if needed ⭐⭐⭐⭐

If you want to provide user feedback about connection quality:
- Add a "last packet" timestamp to UI
- Show connection quality indicator
- Don't make binary connected/disconnected determination
- Simple, honest, no false positives

**Example UI additions**:
```
GCM Node ID: !89a857a9
Last Seen: 5 seconds ago [●]    (green)
Last Seen: 45 seconds ago [●]   (yellow)
Last Seen: 2 minutes ago [●]    (gray)
```

---

## Conclusion

**Bottom Line**: The current system with reconnection-only detection is the **correct design** given the Meshtastic protocol's capabilities over serial UART.

**Why previous attempt likely failed**:
- Attempted to solve a problem that the protocol doesn't provide tools for
- Timeout-based detection is inherently unreliable with non-deterministic packet timing
- False positives likely occurred, making the feature unreliable

**Why not try again**:
- Fundamental protocol limitations haven't changed
- Risk of introducing bugs and complexity
- Marginal benefit (knowing disconnect 1-2 minutes sooner)
- Current reconnection detection is perfect

**When to reconsider**:
- If Meshtastic protocol adds heartbeat acknowledgment
- If switching from serial to WiFi/BLE (which have disconnect signaling)
- If user reports specific issues with current behavior
- If we can modify GCM firmware to send periodic keepalives

---

## Alternative: ESP-NOW Connection Status

**Interesting observation**: ESP-NOW *does* have disconnect detection because:
1. GCI sends heartbeat every 10 seconds
2. GCD monitors heartbeat arrival
3. Reliable because it's point-to-point

**From espnow_handler.cpp:368**:
```cpp
case ESPNOW_MSG_HEARTBEAT: {
    // GCI actively sends heartbeats
    // We can reliably detect if they stop
}
```

This works because:
- Known peer (one GCI)
- Controlled timing (10 second interval)
- Direct connection (not mesh-routed)
- Protocol designed for it

**Contrast with Meshtastic**:
- Unknown number of mesh nodes
- Unpredictable timing
- Mesh-routed (multi-hop)
- Protocol not designed for client disconnect detection over serial

---

**Recommendation**: Keep the current system. It's working correctly given the protocol's design.
