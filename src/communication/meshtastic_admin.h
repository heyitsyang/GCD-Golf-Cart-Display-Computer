#ifndef MESHTASTIC_ADMIN_H
#define MESHTASTIC_ADMIN_H

#include <Arduino.h>
#include <stdint.h>
#include "meshtastic/config.pb.h"
#include "meshtastic/mesh.pb.h"
#include "meshtastic/portnums.pb.h"

// Send an admin command to reboot the Meshtastic radio
// seconds: number of seconds until reboot (0 = immediate, <0 = cancel reboot)
// Returns true if the command was sent successfully, false otherwise
bool mt_send_admin_reboot(int32_t seconds = 0);

// Set the position configuration on the Meshtastic radio
// config: the position configuration to set
// Returns true if the command was sent successfully, false otherwise
bool mt_set_position_config(const meshtastic_Config_PositionConfig *config);

// GPS configuration settings that we want to enforce
struct GpsConfigSettings {
    meshtastic_Config_PositionConfig_GpsMode gps_mode;
    bool fixed_position;
    uint32_t gps_update_interval;
};

// Initialize GPS configuration on boot
// Sends desired GPS config to the Meshtastic radio
// Called by system task every 100ms, runs once after connection is established
// Uses polling to avoid stack overflow that would occur if called from connection callback
void initGpsConfigOnBoot();

// Capture GCM node ID once after connection is established
// Called by system task polling
// Note: Only node ID is available - GCM firmware doesn't send DeviceMetadata messages
void requestMetadataOnce();

// Reset GPS update interval to default before entering sleep mode
// Sets gps_update_interval to 0 (which resets to default 2 minute interval)
// Should be called after SLEEP_PIN goes HIGH, before entering deep sleep
// Returns true if the command was sent successfully, false otherwise
bool resetGpsIntervalBeforeSleep();

// Admin portnum callback to handle ADMIN_APP messages
// Note: Currently a placeholder - kept for future admin message handling
// Must be registered with set_portnum_callback() in setup
void admin_portnum_callback(uint32_t from, uint32_t to, uint8_t channel,
                           meshtastic_PortNum port, meshtastic_Data_payload_t *payload);

// Callback from mt_protocol.cpp when FromRadio.config (position) is received
// Called by mt_protocol.cpp line 195 when position config is received from radio
// Note: Currently unused - GCM firmware doesn't send position config during node report
// Must exist to satisfy linker since mt_protocol.cpp unconditionally calls it
void handlePositionConfigResponse(meshtastic_Config_PositionConfig *config);

// Callback from mt_protocol.cpp when FromRadio.metadata is received
// Called by mt_protocol.cpp line 488 when metadata is received from radio
// Note: Currently unused - GCM firmware doesn't send metadata
// Must exist to satisfy linker since mt_protocol.cpp unconditionally calls it
void handleDeviceMetadata(meshtastic_DeviceMetadata *metadata);

// Callback from mt_protocol.cpp when FromRadio.my_info is received
// Captures node ID (node number)
void handleMyNodeInfo(meshtastic_MyNodeInfo *myNodeInfo);

// Callback from mt_protocol.cpp when GCM reboots
// Reset state to allow re-requesting metadata after reconnection
void handleGcmRebooted();

#endif // MESHTASTIC_ADMIN_H
