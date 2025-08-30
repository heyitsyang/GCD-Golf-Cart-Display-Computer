/*
* mt_wifi.cpp
*  - modified for ESP32 (YS)
*  - siplified by removing #define clutter (YS)
*/

#include "mt_internals.h"

HardwareSerial meshSerial(2);

void mt_serial_init(int8_t rx_pin, int8_t tx_pin, uint32_t baud) {
  meshSerial.begin(baud, SERIAL_8N1, rx_pin, tx_pin);

  // used in other modules
  mt_wifi_mode = false;
  mt_serial_mode = true;
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
  return true;  // It's easy being a serial interface
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
