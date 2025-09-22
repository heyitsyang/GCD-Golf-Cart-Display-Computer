/*
* mt_wifi_esp32.cpp
* ESP32-specific WiFi implementation for Meshtastic communication
* Golf Cart Project Customization
*
* This file contains ESP32-specific WiFi modifications.
* WiFi support is disabled by default for this project.
*/

//#define MT_WIFI_SUPPORTED  // Uncomment to enable WiFi support
#ifdef MT_WIFI_SUPPORTED

#include <WiFi.h>
#include "mt_internals.h"

// Connection timeouts
#define CONNECT_TIMEOUT (10 * 1000)
#define IDLE_TIMEOUT (65 * 1000)

// Default Meshtastic AP settings
#define RADIO_IP "192.168.42.1"
#define RADIO_PORT 4403
#define UNUSED_WIFI_STATUS 254

uint8_t last_wifi_status;
uint32_t next_connect_attempt;

WiFiClient client;
const char* ssid;
const char* password;
bool can_send;

void mt_wifi_init(const char * ssid_, const char * password_) {
    // ESP32 WiFi initialization (no hardware pins needed)
    next_connect_attempt = 0;
    last_wifi_status = UNUSED_WIFI_STATUS;
    ssid = ssid_;
    password = password_;
    can_send = false;
    mt_wifi_mode = true;
    mt_serial_mode = false;
}

void print_wifi_status() {
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);

    long rssi = WiFi.RSSI();
    Serial.print("Signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
}

bool open_tcp_connection() {
    can_send = client.connect(RADIO_IP, RADIO_PORT);
    if (can_send) {
        d("TCP connection established");
    } else {
        d("Failed to establish TCP connection");
    }
    return can_send;
}

bool mt_wifi_loop(uint32_t now) {
    uint8_t wifi_status = WiFi.status();

    if (now >= next_connect_attempt) {
        last_wifi_status = UNUSED_WIFI_STATUS;
        wifi_status = WL_IDLE_STATUS;
    }

    if (wifi_status == last_wifi_status) return can_send;
    last_wifi_status = wifi_status;

    switch (wifi_status) {
        case WL_NO_SHIELD:
            d("No WiFi shield detected");
            while(true);
        case WL_CONNECT_FAILED:
        case WL_CONNECTION_LOST:
        case WL_IDLE_STATUS:
            delay(CONNECT_TIMEOUT);
            next_connect_attempt = now + CONNECT_TIMEOUT;
            d("Attempting to connect to WiFi...");

            if (password == NULL) {
                WiFi.begin(ssid);
            } else {
                WiFi.begin(ssid, password);
            }
            can_send = false;
            return false;
        case WL_CONNECTED:
#ifdef MT_DEBUGGING
            print_wifi_status();
#endif
            mt_wifi_reset_idle_timeout(now);
            open_tcp_connection();
            return can_send;
        case WL_DISCONNECTED:
            can_send = false;
            return false;
        case WL_NO_SSID_AVAIL:
        case WL_SCAN_COMPLETED:
        default:
#if MT_DEBUGGING
            Serial.print("Unknown WiFi status ");
            Serial.println(wifi_status);
#endif
            while(true);
    }
}

size_t mt_wifi_check_radio(char * buf, size_t space_left) {
    if (!client.connected()) {
        d("Lost TCP connection");
        return 0;
    }
    size_t bytes_read = 0;
    while (client.available()) {
        char c = client.read();
        *buf++ = c;
        if (++bytes_read >= space_left) {
            d("TCP overflow");
            client.stop();
            break;
        }
    }
    return bytes_read;
}

bool mt_wifi_send_radio(const char * buf, size_t len) {
    if (!client.connected()) {
        d("Lost TCP connection? Attempting to reconnect...");
        if (!open_tcp_connection()) return false;
    }

    size_t wrote = client.write(buf, len);
    if (wrote == len) return true;

#ifdef MT_DEBUGGING
    Serial.print("Tried to send radio ");
    Serial.print(len);
    Serial.print(" but actually sent ");
    Serial.println(wrote);
#endif
    client.stop();
    return false;
}

void mt_wifi_reset_idle_timeout(uint32_t now) {
    next_connect_attempt = now + IDLE_TIMEOUT;
}

#endif // MT_WIFI_SUPPORTED