#ifndef CONFIG_CALLBACK_H
#define CONFIG_CALLBACK_H

#include "meshtastic/config.pb.h"
#include "meshtastic/mesh.pb.h"
#include "meshtastic/channel.pb.h"

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

// Callback function for channel info received from GCM (boot config dump or admin response)
// Called from handle_channel_tag() in mt_protocol.cpp for each FromRadio_channel_tag packet
extern void handleChannelResponse(meshtastic_Channel *channel);

// Callback function for GCM reboot detection
// This will be called from mt_protocol.cpp when rebooted_tag is received
extern void handleGcmRebooted();

// Callback for node info packets — updates the short-name cache.
// Called from handle_node_info() for every NodeInfo packet that has a user record.
extern void handleNodeInfo(uint32_t nodeNum, const char* shortName);

// Callback function for handshake completion
// Called from mt_protocol.cpp when config_complete_id (tag 7) is received
// Signals that GCM is ready to route packets OTA
extern void handleConfigComplete();

#endif // CONFIG_CALLBACK_H
