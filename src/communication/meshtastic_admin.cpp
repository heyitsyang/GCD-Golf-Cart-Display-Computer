#include "meshtastic_admin.h"
#include "Meshtastic.h"
#include "meshtastic/mesh.pb.h"
#include "meshtastic/admin.pb.h"
#include "meshtastic/portnums.pb.h"
#include "pb_encode.h"

// External declarations from mt_protocol.cpp in meshtastic library
extern uint32_t my_node_num;

// Forward declaration - this function exists in mt_protocol.cpp
extern bool _mt_send_toRadio(meshtastic_ToRadio toRadio);

bool mt_send_admin_reboot(int32_t seconds) {
    // Create the admin message
    meshtastic_AdminMessage adminMsg = meshtastic_AdminMessage_init_default;
    adminMsg.which_payload_variant = meshtastic_AdminMessage_reboot_seconds_tag;
    adminMsg.reboot_seconds = seconds;

    // Encode the admin message into a temporary buffer
    pb_byte_t admin_buf[256];
    pb_ostream_t admin_stream = pb_ostream_from_buffer(admin_buf, sizeof(admin_buf));

    if (!pb_encode(&admin_stream, meshtastic_AdminMessage_fields, &adminMsg)) {
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

    Serial.print("\nSending admin reboot command to Meshtastic radio (");
    Serial.print(seconds);
    Serial.println(" seconds delay)");

    return _mt_send_toRadio(toRadio);
}
