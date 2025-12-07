#ifndef CONFIG_CALLBACK_H
#define CONFIG_CALLBACK_H

#include "meshtastic/config.pb.h"
#include "meshtastic/mesh.pb.h"

// Callback function for position config responses
// This will be called from mt_protocol.cpp when a position config is received
extern void handlePositionConfigResponse(meshtastic_Config_PositionConfig *config);

// Callback function for device metadata (firmware version, hardware model, etc.)
// Called by mt_protocol.cpp line 488 when metadata is received from radio
// Must exist to satisfy linker since mt_protocol.cpp unconditionally calls it
extern void handleDeviceMetadata(meshtastic_DeviceMetadata *metadata);

// Callback function for my node info (node ID, reboot count, etc.)
// This will be called from mt_protocol.cpp when my node info is received
extern void handleMyNodeInfo(meshtastic_MyNodeInfo *myNodeInfo);

// Callback function for GCM reboot detection
// This will be called from mt_protocol.cpp when rebooted_tag is received
extern void handleGcmRebooted();

#endif // CONFIG_CALLBACK_H
