#!/usr/bin/env python3
"""
Verification script to ensure Meshtastic structure is correctly configured
Run this after updates or if you encounter compilation issues
"""

import os
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.parent.parent.parent
PLATFORMIO_INI = PROJECT_ROOT / "platformio.ini"
ESP32_OVERRIDES = PROJECT_ROOT / "lib" / "meshtastic_customizations" / "esp32_overrides"
MESHTASTIC_LIB = PROJECT_ROOT / "lib" / "meshtastic-arduino_src"

def test_platformio_config():
    """Test that platformio.ini is correctly configured for current approach"""
    print("📋 Testing platformio.ini configuration...")

    if not PLATFORMIO_INI.exists():
        print("  ❌ platformio.ini not found")
        return False

    with open(PLATFORMIO_INI, 'r') as f:
        content = f.read()

    # Check that lib_extra_dirs is NOT present (we're using direct file placement now)
    if "lib_extra_dirs" in content and "lib/meshtastic_customizations/esp32_overrides" in content:
        print("  ⚠️  Old lib_extra_dirs configuration found - this approach was changed")
        print("  ℹ️  Current approach uses direct file placement in main library")

    # Check for essential sections
    if "lib_deps" not in content:
        print("  ❌ lib_deps section not found")
        return False

    if "[env:cyd]" not in content:
        print("  ❌ [env:cyd] section not found")
        return False

    print("  ✅ platformio.ini has required sections")
    return True

def test_esp32_implementations():
    """Test that ESP32 implementations are correctly placed"""
    print("🔧 Testing ESP32 implementation files...")

    # Test 1: Check master copies in esp32_overrides
    master_files = [
        "mt_serial_esp32.cpp",
        "mt_wifi_esp32.cpp"
    ]

    for file in master_files:
        file_path = ESP32_OVERRIDES / file
        if not file_path.exists():
            print(f"  ❌ Missing master file: {file}")
            return False
        print(f"  ✅ Master file exists: {file}")

    # Test 2: Check active implementations in main library
    active_files = [
        ("mt_serial.cpp", ["mt_serial_init", "mt_serial_send_radio", "mt_serial_check_radio"]),
        ("mt_wifi.cpp", ["mt_wifi_init", "mt_wifi_send_radio", "mt_wifi_check_radio"])
    ]

    for file, required_functions in active_files:
        file_path = MESHTASTIC_LIB / file
        if not file_path.exists():
            print(f"  ❌ Missing active implementation: {file}")
            return False

        with open(file_path, 'r') as f:
            content = f.read()

        # Check for ESP32-specific markers
        if "ESP32" not in content and "HardwareSerial(2)" not in content:
            print(f"  ⚠️  {file} may not be ESP32 customized")

        # Check for required functions
        missing_functions = []
        for func in required_functions:
            if func not in content:
                missing_functions.append(func)

        if missing_functions:
            print(f"  ❌ Missing functions in {file}: {missing_functions}")
            return False

        print(f"  ✅ Active implementation correct: {file}")

    return True

def test_old_files_backed_up():
    """Check status of backup files (non-critical)"""
    print("💾 Checking backup file status...")

    backup_files = [
        "mt_serial_ys_modified.cpp.backup",
        "mt_wifi_ys_modified.cpp.backup"
    ]

    backup_found = False
    for file in backup_files:
        file_path = MESHTASTIC_LIB / file
        if not file_path.exists():
            print(f"  ⚠️  Backup file not found: {file} (may have been cleaned up)")
        else:
            print(f"  ✅ Backup preserved: {file}")
            backup_found = True

    # Check that active versions are removed
    active_files = [
        "mt_serial_ys_modified.cpp",
        "mt_wifi_ys_modified.cpp"
    ]

    for file in active_files:
        file_path = MESHTASTIC_LIB / file
        if file_path.exists():
            print(f"  ⚠️  Old active file still exists: {file} (consider removing)")
        else:
            print(f"  ✅ Old active file removed: {file}")

    if not backup_found:
        print("  ℹ️  No backup files found - likely cleaned up after successful migration")

    return True  # Always return True since backups are non-critical

def test_directory_structure():
    """Test that directory structure is correct"""
    print("📁 Testing directory structure...")

    required_dirs = [
        ESP32_OVERRIDES,
        PROJECT_ROOT / "lib" / "meshtastic_customizations" / "update_scripts",
        PROJECT_ROOT / "lib" / "meshtastic_customizations" / "upstream_backup"
    ]

    for dir_path in required_dirs:
        if not dir_path.exists():
            print(f"  ❌ Missing directory: {dir_path}")
            return False
        print(f"  ✅ Directory exists: {dir_path.name}")

    return True

def main():
    """Run all tests"""
    print("🚀 Verifying Meshtastic Structure Configuration")
    print("=" * 50)

    tests = [
        test_directory_structure,
        test_platformio_config,
        test_esp32_implementations,
        test_old_files_backed_up
    ]

    all_passed = True

    for test in tests:
        if not test():
            all_passed = False
        print()

    print("=" * 50)
    if all_passed:
        print("✅ All verification tests passed! Meshtastic structure is correctly configured!")
        print()
        print("📋 Next steps:")
        print("  1. Run 'pio run' to test compilation")
        print("  2. If compilation succeeds, you can delete .backup files")
        print("  3. Use 'python3 update_meshtastic.py' to update protobuf definitions")
        print("  4. Your ESP32 customizations are preserved in clean, separate files")
    else:
        print("❌ Some tests failed. Please check the issues above.")
        print("💡 You can restore backup files if needed:")
        print("  mv lib/meshtastic-arduino_src/*.backup lib/meshtastic-arduino_src/")
        print("  (remove .backup extension)")

    return all_passed

if __name__ == "__main__":
    main()