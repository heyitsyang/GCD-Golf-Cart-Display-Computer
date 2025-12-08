#ifndef CONFIG_H
#define CONFIG_H

// Debug Settings
#define DEBUG_TOUCH_SCREEN 0
#define DEBUG_GPS 0
// MT_DEBUGGING - LEAVE UNDEFINED (not 0) to disable low-level Meshtastic protocol debugging
// Upstream code uses #ifdef (not #if), so defining it to 0 still enables it!
// To enable: uncomment and set to 1
// #define MT_DEBUGGING 1
#define DEBUG_ESP32_SLEEP 0
#define DEBUG_ESPNOW 0
#define DEBUG_MESHTASTIC_CONNECTION 0  // GCM connection/reconnection events

// Speaker pin & default settings
#define SPEAKER_PIN 26
#define BEEP_FREQUENCY_HZ 2500
#define BEEP_DURATION_MS 100
#define BEEP_SPACING_MS BEEP_DURATION/2
#define SPEAKER_VOLUME 20

// Sleep pin (sleeps when LOW)
#define SLEEP_PIN 35

// Touch Screen pins
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// Touch rotation values
#define TOUCH_ROTATION_0 0
#define TOUCH_ROTATION_90 1
#define TOUCH_ROTATION_180 2
#define TOUCH_ROTATION_270 3

// Display dimensions
#define TOUCH_WIDTH 320
#define TOUCH_HEIGHT 240
#define TFT_WIDTH 240
#define TFT_HEIGHT 320

// Backlight configuration
#define TFT_BACKLIGHT_PIN 21  //used internally to CYD PCB
#define LEDC_CHANNEL_0 0
#define LEDC_BASE_FREQ 5000
#define LEDC_TIMER_12_BIT 12
#define MAX_BACKLIGHT_VALUE 255

// Speaker LEDC configuration
#define SPEAKER_LEDC_CHANNEL 1
#define SPEAKER_LEDC_TIMER_BIT 8

// LVGL buffer configuration
#define NUM_BUFS 2
#define DRAW_BUF_SIZE (TFT_WIDTH * 30 * sizeof(lv_color_t))
// This gives 240 * 30 * 2 = 14,400 bytes

// Meshtastic configuration
#define MT_SERIAL_TX_PIN 22
#define MT_SERIAL_RX_PIN 27
#define MT_DEV_BAUD_RATE 9600
#define MAX_MESHTASTIC_PAYLOAD 237
#define HOT_PKT_HEADER_OFFSET 5
#define SEND_PERIOD 300

// GPS configuration
#define GPS_RX_PIN 03
#define GPS_TX_PIN 01
#define GPS_BAUD 9600
#define MAX_GPS_TIME_STALENESS_SECS 60  // Show "NO GPS" if no time update for this many seconds
#define MIN_SPEED_FILTER_MPH 2.5        // Speeds below this are treated as 0 to filter GPS dither

// ESP-NOW configuration
#define ESPNOW_CHANNEL 1
#define ESPNOW_MAX_PEER_NUM 6
#define ESPNOW_MAX_PAYLOAD 240  // Max payload after wrapper overhead subtracted (ESP-NOW limit: 250 bytes, wrapper: 9 bytes, payload: 241 bytes)
#define ESPNOW_QUEUE_SIZE 10
#define ESPNOW_SEND_RETRY_COUNT 3
#define ESPNOW_SEND_RETRY_DELAY 100
#define ESPNOW_HEARTBEAT_INTERVAL 10000
#define ESPNOW_GPS_SEND_INTERVAL 60000
#define ESPNOW_PEER_TIMEOUT 40000  // 40 seconds - 4x heartbeat interval

// Default location (for sunrise/sunset before GPS lock)
#define MY_LATITUDE 28.8522f
#define MY_LONGITUDE -82.0028f

// UI update flag timing (milliseconds)
#define NEW_RX_DATA_FLAG_RESET_TIME 5000  // Auto-reset flag after 5 seconds

// Inactivity timeout configuration
#define SCREEN_INACTIVITY_TIMEOUT_MS (1 * 60 * 1000)  // 1 minutes
//#define SCREEN_INACTIVITY_TIMEOUT_MS (10 * 1000)  // 10 secs for debugging

// Sleep configuration
#define SLEEP_CHECK_INTERVAL_MS 100  // How often system task checks SLEEP_PIN (ms)

// Task Stack Sizes (in bytes)
#define GPS_TASK_STACK_SIZE 4096
#define GUI_TASK_STACK_SIZE 8192
#define MESHTASTIC_TASK_STACK_SIZE 4096
#define MESHTASTIC_CALLBACK_TASK_STACK_SIZE 6144
#define EEPROM_TASK_STACK_SIZE 2048
#define SYSTEM_TASK_STACK_SIZE 4096  // Increased for GPS config init with debug output
#define ESPNOW_TASK_STACK_SIZE 4096

// Task Priorities
#define GUI_TASK_PRIORITY 3
#define GPS_TASK_PRIORITY 2
#define MESHTASTIC_TASK_PRIORITY 2
#define MESHTASTIC_CALLBACK_TASK_PRIORITY 2
#define ESPNOW_TASK_PRIORITY 2
#define EEPROM_TASK_PRIORITY 1
#define SYSTEM_TASK_PRIORITY 1

#endif // CONFIG_H