#include "meshtastic_admin.h"
#include "Meshtastic.h"
#include "meshtastic/mesh.pb.h"
#include "meshtastic/admin.pb.h"
#include "meshtastic/config.pb.h"
#include "meshtastic/portnums.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "globals.h"

// External declarations from mt_protocol.cpp in meshtastic library
extern uint32_t my_node_num;

// Forward declaration - this function exists in mt_protocol.cpp
extern bool _mt_send_toRadio(meshtastic_ToRadio toRadio);

// Desired GPS configuration settings
static const GpsConfigSettings desiredGpsConfig = {
    .gps_mode = meshtastic_Config_PositionConfig_GpsMode_ENABLED,
    .fixed_position = false,
    .gps_update_interval = 8
};

// State tracking for GPS config initialization
static bool gpsConfigInitialized = false;
static uint8_t configUpdateRetries = 0;
static const uint8_t MAX_RETRIES = 100;  // 10 seconds at 100ms intervals

// Helper function to send admin messages
static bool sendAdminMessage(meshtastic_AdminMessage *adminMsg) {
    // Encode the admin message into a temporary buffer
    pb_byte_t admin_buf[256];
    pb_ostream_t admin_stream = pb_ostream_from_buffer(admin_buf, sizeof(admin_buf));

    if (!pb_encode(&admin_stream, meshtastic_AdminMessage_fields, adminMsg)) {
        Serial.println("Failed to encode AdminMessage");
        return false;
    }

    // Create a MeshPacket with the admin message as payload
    meshtastic_MeshPacket meshPacket = meshtastic_MeshPacket_init_default;
    meshPacket.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    meshPacket.id = random(0x7FFFFFFF);
    meshPacket.decoded.portnum = meshtastic_PortNum_ADMIN_APP;
    meshPacket.to = my_node_num;  // Send to the connected radio
    meshPacket.channel = 0;
    meshPacket.decoded.payload.size = admin_stream.bytes_written;
    memcpy(meshPacket.decoded.payload.bytes, admin_buf, admin_stream.bytes_written);

    // Wrap in ToRadio and send
    meshtastic_ToRadio toRadio = meshtastic_ToRadio_init_default;
    toRadio.which_payload_variant = meshtastic_ToRadio_packet_tag;
    toRadio.packet = meshPacket;

    return _mt_send_toRadio(toRadio);
}

bool mt_send_admin_reboot(int32_t seconds) {
    meshtastic_AdminMessage adminMsg = meshtastic_AdminMessage_init_default;
    adminMsg.which_payload_variant = meshtastic_AdminMessage_reboot_seconds_tag;
    adminMsg.reboot_seconds = seconds;

    Serial.print("\nSending admin reboot command to Meshtastic radio (");
    Serial.print(seconds);
    Serial.println(" seconds delay)");

    return sendAdminMessage(&adminMsg);
}

bool mt_set_position_config(const meshtastic_Config_PositionConfig *config) {
    meshtastic_AdminMessage adminMsg = meshtastic_AdminMessage_init_default;
    adminMsg.which_payload_variant = meshtastic_AdminMessage_set_config_tag;
    adminMsg.set_config.which_payload_variant = meshtastic_Config_position_tag;
    adminMsg.set_config.payload_variant.position = *config;

    return sendAdminMessage(&adminMsg);
}

// Admin portnum callback to handle ADMIN_APP messages
// Currently a placeholder - kept for potential future admin message handling
void admin_portnum_callback(uint32_t from, uint32_t to, uint8_t channel,
                           meshtastic_PortNum port, meshtastic_Data_payload_t *payload) {
    // Only process actual ADMIN_APP messages (port 6)
    if (port != meshtastic_PortNum_ADMIN_APP) {
        return;
    }

    // If we ever receive an ADMIN_APP message, log it for debugging
    meshtastic_AdminMessage adminMsg = meshtastic_AdminMessage_init_default;
    pb_istream_t stream = pb_istream_from_buffer(payload->bytes, payload->size);

    if (pb_decode(&stream, meshtastic_AdminMessage_fields, &adminMsg)) {
        Serial.printf("*** Received ADMIN_APP message, variant=%d ***\n", adminMsg.which_payload_variant);
    }
}

// Callback from mt_protocol.cpp when FromRadio.config (position) is received
// Called by mt_protocol.cpp line 195 when position config is received from radio
// Note: Currently unused - GCM firmware doesn't send position config during node report
// Must exist to satisfy linker since mt_protocol.cpp unconditionally calls it
void handlePositionConfigResponse(meshtastic_Config_PositionConfig *config) {
    // Empty placeholder - GCM never sends position config
}

// Callback from mt_protocol.cpp when FromRadio.metadata is received
// Called by mt_protocol.cpp line 488 when metadata is received from radio
// Note: Currently unused - GCM firmware doesn't send metadata
// Must exist to satisfy linker since mt_protocol.cpp unconditionally calls it
void handleDeviceMetadata(meshtastic_DeviceMetadata *metadata) {
    // Empty placeholder - GCM never sends metadata
}

// Callback from mt_protocol.cpp when FromRadio.my_info is received
// Captures node ID (node number)
void handleMyNodeInfo(meshtastic_MyNodeInfo *myNodeInfo) {
    if (myNodeInfo == nullptr) {
        return;
    }

    // Convert node number to hex string with ! prefix (e.g., !a1b2c3d4)
    uint32_t nodeNum = myNodeInfo->my_node_num;
    char nodeIdStr[12];
    snprintf(nodeIdStr, sizeof(nodeIdStr), "!%08x", nodeNum);

    set_var_gcm_node_id(nodeIdStr);
    Serial.print("GCM Node ID: ");
    Serial.println(nodeIdStr);
}

// Callback from mt_protocol.cpp when GCM reboots
// Reset state to allow re-capturing node ID and resending wake notification after reconnection
void handleGcmRebooted() {
    Serial.println("*** GCM REBOOTED - Resetting state for reconnection ***");

    // Clear the stored node ID to indicate stale data
    set_var_gcm_node_id("");

    // Reset wake notification flag so it will be sent again on reconnection
    wakeNotificationSent = false;
}

void initGpsConfigOnBoot() {
    // Only run once successfully
    if (gpsConfigInitialized) {
        return;
    }

    // Wait for Meshtastic connection
    if (not_yet_connected) {
        return;
    }

    // Connection established - send desired GPS config
    // We cannot read current config (GCM doesn't send it), so we just write our desired settings
    // This is idempotent - Meshtastic firmware handles repeated identical writes gracefully

    // Create a full position config with desired values
    meshtastic_Config_PositionConfig config = meshtastic_Config_PositionConfig_init_default;
    config.gps_mode = desiredGpsConfig.gps_mode;
    config.fixed_position = desiredGpsConfig.fixed_position;
    config.gps_update_interval = desiredGpsConfig.gps_update_interval;

    // Send the config - retry with limit
    if (configUpdateRetries >= MAX_RETRIES) {
        Serial.println("GPS Config Init: Failed after max retries - giving up");
        gpsConfigInitialized = true;  // Give up and don't retry forever
        return;
    }

    if (mt_set_position_config(&config)) {
        Serial.println("GPS Config Init: Complete");
        gpsConfigInitialized = true;
    } else {
        configUpdateRetries++;
        if (configUpdateRetries == 1) {
            Serial.println("GPS Config Init: Sending config to Meshtastic radio...");
        }
    }
}

// Capture node ID once after GCM connection is established
// Called by system task polling
// Note: Only node ID is available - GCM firmware doesn't send DeviceMetadata messages
void requestMetadataOnce() {
    static bool nodeIdCaptured = false;

    // Only run once, and only after connection is established
    if (!nodeIdCaptured && !not_yet_connected) {
        // Capture node ID from my_node_num (set during connection handshake)
        if (my_node_num != 0) {
            char nodeIdStr[12];
            snprintf(nodeIdStr, sizeof(nodeIdStr), "!%08x", my_node_num);
            set_var_gcm_node_id(nodeIdStr);
            Serial.print("GCM Node ID: ");
            Serial.println(nodeIdStr);
            nodeIdCaptured = true;
        }
    }
}

bool resetGpsIntervalBeforeSleep() {
    // Create position config with interval set to 0 (default = 2 minutes)
    meshtastic_Config_PositionConfig config = meshtastic_Config_PositionConfig_init_default;
    config.gps_mode = desiredGpsConfig.gps_mode;  // Keep GPS enabled
    config.fixed_position = desiredGpsConfig.fixed_position;  // Keep fixed_position setting
    config.gps_update_interval = 0;  // 0 = reset to default (2 minutes)

    Serial.println("Resetting GPS update interval to default (2 min) before sleep...");

    if (mt_set_position_config(&config)) {
        Serial.println("GPS interval reset successful");
        return true;
    } else {
        Serial.println("GPS interval reset failed");
        return false;
    }
}
