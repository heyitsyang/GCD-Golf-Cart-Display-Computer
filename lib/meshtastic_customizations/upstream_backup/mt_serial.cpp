/*
* mt_serial_esp32.cpp
* ESP32-specific serial implementation for Meshtastic communication
* Golf Cart Project Customization
*
* This file contains ESP32-specific modifications that replace the generic
* Meshtastic serial implementation with optimized ESP32 UART handling.
*/

#include "mt_internals.h"

HardwareSerial meshSerial(2);  // Use UART2 on ESP32

void mt_serial_init(int8_t rx_pin, int8_t tx_pin, uint32_t baud) {
    // ESP32-specific serial initialization using UART2
    meshSerial.begin(baud, SERIAL_8N1, rx_pin, tx_pin);

    // Configure mode flags for other modules
    mt_wifi_mode = false;
    mt_serial_mode = true;
}

void mt_serial_end() {
    meshSerial.end();
}

bool mt_serial_send_radio(const char * buf, size_t len) {
    size_t wrote = meshSerial.write(buf, len);
    if (wrote == len) return true;

#ifdef MT_DEBUGGING
    Serial.print("Tried to send radio ");
    Serial.print(len);
    Serial.print(" but actually sent ");
    Serial.println(wrote);
#endif

    return false;
}

bool mt_serial_loop() {
    return true;  // Serial interface requires no maintenance
}

size_t mt_serial_check_radio(char * buf, size_t space_left) {
    size_t bytes_read = 0;
    while (meshSerial.available()) {
        char c = meshSerial.read();
        *buf++ = c;
        if (++bytes_read >= space_left) {
            d("Serial overflow");
            break;
        }
    }
    return bytes_read;
}