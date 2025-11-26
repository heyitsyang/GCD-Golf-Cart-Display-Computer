#ifndef MESHTASTIC_ADMIN_H
#define MESHTASTIC_ADMIN_H

#include <Arduino.h>
#include <stdint.h>
#include "meshtastic/config.pb.h"

// Send an admin command to reboot the Meshtastic radio
// seconds: number of seconds until reboot (0 = immediate, <0 = cancel reboot)
// Returns true if the command was sent successfully, false otherwise
bool mt_send_admin_reboot(int32_t seconds = 0);

// Request the current position configuration from the Meshtastic radio
// The response will be received via the config callback
// Returns true if the request was sent successfully, false otherwise
bool mt_request_position_config();

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

// Initialize GPS configuration on boot (waits for Meshtastic connection)
// This function checks the current GPS config and updates it only if needed
void initGpsConfigOnBoot();

#endif // MESHTASTIC_ADMIN_H
