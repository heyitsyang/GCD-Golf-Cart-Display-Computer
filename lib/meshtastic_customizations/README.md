# Meshtastic Customizations for Golf Cart Project

This directory contains ESP32-specific customizations for the Meshtastic library that are preserved during updates.

## Directory Structure

```
meshtastic_customizations/
├── esp32_overrides/          # Clean ESP32-specific implementations
│   ├── mt_serial_esp32.cpp   # ESP32 UART2 serial implementation
│   └── mt_wifi_esp32.cpp     # ESP32 WiFi implementation (disabled)
├── upstream_backup/          # Backup of library before last update
├── update_scripts/           # Automated update tools
│   └── update_meshtastic.py  # Main update script
└── README.md                # This file
```

## Current Customizations

### ESP32 Serial Implementation (`mt_serial_esp32.cpp`)
- Uses ESP32 UART2 (`HardwareSerial(2)`) for Meshtastic communication
- Optimized for ESP32 pin configuration
- Removes generic platform abstractions
- Sets proper mode flags for serial-only operation

### ESP32 WiFi Implementation (`mt_wifi_esp32.cpp`)
- ESP32-specific WiFi handling (currently disabled)
- Removes hardware pin dependencies needed by other platforms
- Uses ESP32 native WiFi library

### Critical Bug Fixes (Applied via Patches)

#### Rebooted Tag Return Statement Fix (`patches/mt_protocol_rebooted_tag_fix.patch`)
**Upstream Bug**: Missing `return true;` statement after `rebooted_tag` case (tag 8) in mt_protocol.cpp
**Impact**: Causes fall-through to `moduleConfig_tag` case, leading to crashes when GCM reboots
**Symptom**: `InstrFetchProhibited` exception at address 0x00000000
**Fix**: Adds `return true;` statement at mt_protocol.cpp line 686 (consistent with all other cases)
**Status**: Upstream bug not fixed as of 2025-12 (confirmed in github.com/meshtastic/Meshtastic-arduino)
**Applied by**: `patches/apply_patches.py` (automatic during update process)

## Updating Meshtastic Protobuf Definitions

### Automated Update (Recommended)
```bash
cd lib/meshtastic_customizations/update_scripts/
python3 update_meshtastic.py
```

This script will:
1. ✅ Backup current library
2. ⬇️ Download latest Meshtastic-arduino
3. 🔄 Update protobuf definitions
4. 🔧 Apply critical bug fix patches
5. 🔒 Preserve ESP32 customizations
6. ✅ Verify everything is working

### Manual Update Process
1. **Backup current library**: Copy `lib/meshtastic-arduino_src/` to safe location
2. **Download latest**: Get latest from https://github.com/meshtastic/Meshtastic-arduino
3. **Update protobuf files**: Copy all `*.pb.h`, `*.pb.c` files from `src/meshtastic/`
4. **Update core files**: Copy `mt_protocol.cpp`, `mt_internals.h`, `Meshtastic.h`
5. **Apply patches**: Run `python3 patches/apply_patches.py` to apply critical bug fixes
6. **Preserve customizations**: Keep ESP32 customizations from `esp32_overrides/`
7. **Test compilation**: Ensure project still compiles

## Migration from Old Structure

### Before Migration
```
lib/meshtastic-arduino_src/
├── mt_serial_ys_modified.cpp  # Your ESP32 customizations
├── mt_wifi_ys_modified.cpp    # Your ESP32 customizations
├── [various protobuf files]   # Mix of old and current
└── README YS                  # Your notes
```

### After Migration (Current Structure)
```
lib/meshtastic-arduino_src/
├── mt_serial.cpp              # ESP32 implementation (from esp32_overrides)
├── mt_wifi.cpp                # ESP32 implementation (from esp32_overrides)
├── mt_serial_ys_modified.cpp.backup  # Backup of old customizations
├── mt_wifi_ys_modified.cpp.backup    # Backup of old customizations
├── [protobuf files]           # Current upstream definitions
└── README YS                  # Updated notes

lib/meshtastic_customizations/
├── esp32_overrides/           # Master copies of ESP32 implementations
├── update_scripts/            # Automated update tools
└── README.md                  # This documentation
```

The update script automatically copies clean ESP32 implementations from `esp32_overrides/` to the main library as `mt_serial.cpp` and `mt_wifi.cpp`.

## Advantages of New Structure

- ✅ **Update Safety**: Customizations can't be overwritten
- ✅ **Clean Separation**: ESP32 code separated from generic protobuf definitions
- ✅ **Documentation**: Clear tracking of what was customized and why
- ✅ **Automation**: One-command updates with `update_meshtastic.py`
- ✅ **Rollback**: Easy restoration from backup if needed

## Version Tracking

- **Current Meshtastic Version**: Check `upstream_backup/` for last updated version
- **Last Update**: Check file timestamps in `upstream_backup/`
- **Customization Version**: ESP32 implementations in `esp32_overrides/`

## Troubleshooting

### Update Failed
If the automated update fails:
1. Check `upstream_backup/` - your original files are safe
2. Restore manually: `cp -r upstream_backup/* ../meshtastic_serial+wifi/`
3. Report the issue for script improvement

### Compilation Errors After Update
1. Check if protobuf message structures changed
2. Review customizations in `esp32_overrides/` for compatibility
3. Use backup to compare: `diff -r upstream_backup/ ../meshtastic_serial+wifi/`

### Missing Features
If new Meshtastic features aren't working:
1. Check if new protobuf messages were added
2. Verify ESP32 customizations support new message types
3. Update ESP32 implementations if needed

## Contributing

When modifying ESP32 customizations:
1. Edit files in `esp32_overrides/` (not `*_ys_modified.cpp`)
2. Document changes in this README
3. Test with latest protobuf definitions
4. Ensure update script preserves your changes