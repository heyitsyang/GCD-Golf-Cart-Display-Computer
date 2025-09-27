# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a golf cart smart display project built for the ESP32-based Cheap Yellow Display (CYD) 2432S028R. The system integrates GPS navigation, Meshtastic mesh networking, ESP-NOW communication, and a touchscreen UI using LVGL and EEZ Studio Flow.

## Development Commands

### Build and Upload
```bash
# Build the project
pio run

# Upload to device (ensure correct port in platformio.ini)
pio run --target upload

# Monitor serial output
pio device monitor
```

### Configuration
- Monitor port and upload port are set in `platformio.ini` (currently COM12)
- Adjust these settings based on your development environment

## Architecture Overview

### Core Design Principles
- **FreeRTOS Task-Based**: The system uses multiple concurrent FreeRTOS tasks for different subsystems
- **UART0 Split Design**: GPS RX on pin 3, Debug TX on pin 1 (shared serial interface)
- **Modular Structure**: Code is organized into functional modules (hardware, communication, tasks, UI, storage, utils)

### Key Systems

#### 1. Task Architecture (`src/tasks/`)
The system runs 7 concurrent FreeRTOS tasks:

- **GPS Task** (`gps_task.cpp`): Handles NMEAGPS data processing and location calculations
- **GUI Task** (`gui_task.cpp`): Manages LVGL display updates and touch input
- **Meshtastic Task** (`meshtastic_task.cpp`): Handles mesh radio communication
- **Meshtastic Callback Task** (`meshtastic_callback_task.cpp`): Processes incoming mesh messages
- **ESP-NOW Task** (`espnow_task.cpp`): Manages direct ESP32-to-ESP32 communication
- **EEPROM Task** (`eeprom_task.cpp`): Handles persistent storage operations
- **System Task** (`system_task.cpp`): System monitoring and maintenance

#### 2. Communication Systems (`src/communication/`)
- **Meshtastic Integration**: Serial communication with Meshtastic radio module
- **ESP-NOW Handler**: Direct device-to-device WiFi communication
- **Hot Packet Parser**: Custom protocol for weather and venue event data

#### 3. UI System (`src/ui_eez/`)
- **EEZ Studio Flow Integration**: UI designed in EEZ Studio, exported to LVGL code
- **LVGL Framework**: Graphics library for embedded displays
- **Touch Input**: XPT2046 touchscreen controller integration

#### 4. Hardware Abstraction (`src/hardware/`)
- **Display Management**: TFT display and backlight control
- **Touch Interface**: Touchscreen calibration and input processing

### Critical Configuration Files

#### `src/config.h`
Central configuration hub containing:
- Pin assignments for all peripherals
- Task stack sizes and priorities
- Communication protocol settings
- Hardware-specific parameters

#### `platformio.ini`
- Build scripts: `copy_cyd_configs.py`, `fix_lv_dropdown_set_selected.py`, `autoincrement.py`
- Library dependencies for GPS, display, and mesh networking
- ESP32 board configuration for CYD hardware

#### `NECESSARY TEMPLATE FILES/`
Contains essential configuration files:
- `lv_conf.h`: LVGL library configuration
- `User_Setup.h`: TFT_eSPI display driver configuration

### Data Flow and Synchronization

#### FreeRTOS Synchronization
- **Mutexes**: `gpsMutex`, `eepromMutex`, `displayMutex` prevent concurrent access
- **Queues**: Inter-task communication via `eepromWriteQueue`, `meshtasticCallbackQueue`, `espnowRecvQueue`

#### Global State Management (`src/globals.h`)
- Central declaration of all shared variables and handles
- Includes EEZ Studio variables via `get_set_vars.h`
- Separates UI variables from system variables

### Build Process

#### Pre-build Scripts
1. **`copy_cyd_configs.py`**: Copies hardware-specific configs to library dependencies
2. **`fix_lv_dropdown_set_selected.py`**: Patches LVGL dropdown functionality

#### Post-build Scripts
1. **`autoincrement.py`**: Auto-increments build version in `src/version.h`

### Key Integration Points

#### EEZ Studio Flow Integration
- UI designed graphically in EEZ Studio
- Exported as LVGL-compatible C code
- Variables managed through `get_set_vars.h` for two-way binding

#### Meshtastic Protocol
- Custom library integration for mesh networking
- Supports text messaging and telemetry data
- Hot packet system for weather and event data

#### GPS Integration
- Uses NeoGPS library for NMEA sentence parsing
- Moving average filters for speed and heading
- Sunrise/sunset calculations for display brightness

### Development Notes

#### Hardware Constraints
- Limited ESP32 memory requires careful stack size management
- Display updates must be synchronized with LVGL timer tick
- GPS and debug share UART0 in split configuration

#### Communication Protocols
- Meshtastic: Long-range mesh networking (serial interface)
- ESP-NOW: Short-range direct WiFi communication
- Hot Packets: Custom protocol for structured data over Meshtastic

#### Task Priorities
- GUI Task: Priority 3 (highest) for responsive UI
- Communication tasks: Priority 2 for real-time data
- Storage/System tasks: Priority 1 for background operations

### Important File Dependencies
- `src/types.h`: Defines all custom data structures and enums
- `src/prototypes.h`: Function declarations for cross-module communication
- `src/version.h`: Auto-generated version information
- `lib/meshtastic-arduino_src/`: Meshtastic protocol implementation copied from github.com/meshtastic/Meshtastic-arduino
- `lib/meshtastic-custimizarions/`: Customizations of lib/meshtastic-arduino_src code
- # ok, we'll implement option 3, but remember the optiona and this point in out code devleopment so I can ask you to convert to option 1 later if necessary