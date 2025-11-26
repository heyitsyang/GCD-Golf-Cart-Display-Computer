#ifndef CONFIG_CALLBACK_H
#define CONFIG_CALLBACK_H

#include "meshtastic/config.pb.h"

// Callback function for position config responses
// This will be called from mt_protocol.cpp when a position config is received
extern void handlePositionConfigResponse(meshtastic_Config_PositionConfig *config);

#endif // CONFIG_CALLBACK_H
