#ifndef MESHTASTIC_ADMIN_H
#define MESHTASTIC_ADMIN_H

#include <Arduino.h>
#include <stdint.h>

// Send an admin command to reboot the Meshtastic radio
// seconds: number of seconds until reboot (0 = immediate, <0 = cancel reboot)
// Returns true if the command was sent successfully, false otherwise
bool mt_send_admin_reboot(int32_t seconds = 0);

#endif // MESHTASTIC_ADMIN_H
