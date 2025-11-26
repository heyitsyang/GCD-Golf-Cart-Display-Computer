#include "meshtastic_admin.h"
#include "Meshtastic.h"
#include "meshtastic/mesh.pb.h"
#include "meshtastic/admin.pb.h"
#include "meshtastic/config.pb.h"
#include "meshtastic/portnums.pb.h"
#include "pb_encode.h"
#include "globals.h"

// External declarations from mt_protocol.cpp in meshtastic library
extern uint32_t my_node_num;

// Forward declaration - this function exists in mt_protocol.cpp
extern bool _mt_send_toRadio(meshtastic_ToRadio toRadio);

// Desired GPS configuration settings
static const GpsConfigSettings desiredGpsConfig = {
    .gps_mode = meshtastic_Config_PositionConfig_GpsMode_ENABLED,
    .fixed_position = false,
    .gps_update_interval = 5
};

// State tracking for GPS config initialization
static bool gpsConfigInitialized = false;
static bool gpsConfigRequestSent = false;

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

bool mt_request_position_config() {
    meshtastic_AdminMessage adminMsg = meshtastic_AdminMessage_init_default;
    adminMsg.which_payload_variant = meshtastic_AdminMessage_get_config_request_tag;
    adminMsg.get_config_request = meshtastic_AdminMessage_ConfigType_POSITION_CONFIG;

    Serial.println("Requesting position config from Meshtastic radio");

    return sendAdminMessage(&adminMsg);
}

bool mt_set_position_config(const meshtastic_Config_PositionConfig *config) {
    meshtastic_AdminMessage adminMsg = meshtastic_AdminMessage_init_default;
    adminMsg.which_payload_variant = meshtastic_AdminMessage_set_config_tag;
    adminMsg.set_config.which_payload_variant = meshtastic_Config_position_tag;
    adminMsg.set_config.payload_variant.position = *config;

    Serial.println("Setting position config on Meshtastic radio");

    return sendAdminMessage(&adminMsg);
}

// Callback for handling config responses (called from customization layer)
// NOTE: This is called from mt_protocol.cpp context - MUST be minimal and fast!
// Uses queue pattern like text_message_callback to avoid issues
void handlePositionConfigResponse(meshtastic_Config_PositionConfig *config) {
    if (!gpsConfigInitialized && gpsConfigRequestSent && config != nullptr) {
        // Create item on stack (safe in callback context)
        gpsConfigCallbackItem_t item;
        item.config = *config;  // Copy struct to stack

        // Put in queue (non-blocking)
        xQueueSend(gpsConfigCallbackQueue, &item, 0);
    }
}

void initGpsConfigOnBoot() {
    // Only run once
    if (gpsConfigInitialized) {
        return;
    }

    // Wait for Meshtastic connection
    if (not_yet_connected) {
        return;
    }

    // Step 1: Request current config (only once)
    if (!gpsConfigRequestSent) {
        Serial.println("\n=== GPS Config Init: Requesting current config ===");
        if (mt_request_position_config()) {
            gpsConfigRequestSent = true;
        } else {
            Serial.println("Failed to send config request, will retry");
        }
        return;
    }

    // Step 2: Wait for response
    if (!configReceived) {
        return;
    }

    // Step 3: Compare and update if needed
    bool needsUpdate = false;

    if (receivedGpsMode != desiredGpsConfig.gps_mode) {
        needsUpdate = true;
    }

    if (receivedFixedPosition != desiredGpsConfig.fixed_position) {
        needsUpdate = true;
    }

    if (receivedGpsUpdateInterval != desiredGpsConfig.gps_update_interval) {
        needsUpdate = true;
    }

    if (needsUpdate) {
        // Update only the fields we care about in the full config, preserve others
        fullConfig.gps_mode = desiredGpsConfig.gps_mode;
        fullConfig.fixed_position = desiredGpsConfig.fixed_position;
        fullConfig.gps_update_interval = desiredGpsConfig.gps_update_interval;

        mt_set_position_config(&fullConfig);
    }

    gpsConfigInitialized = true;
}
