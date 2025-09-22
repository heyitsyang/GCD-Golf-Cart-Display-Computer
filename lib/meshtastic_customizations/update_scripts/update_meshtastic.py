#!/usr/bin/env python3
"""
Meshtastic Update Script for Golf Cart Project
Safely updates Meshtastic protobuf definitions while preserving ESP32 customizations
"""

import os
import shutil
import subprocess
import tempfile
import zipfile
from urllib.request import urlretrieve
from pathlib import Path

# Configuration
MESHTASTIC_REPO = "https://github.com/meshtastic/Meshtastic-arduino"
PROJECT_ROOT = Path(__file__).parent.parent.parent.parent
MESHTASTIC_LIB = PROJECT_ROOT / "lib" / "meshtastic-arduino_src"
CUSTOMIZATIONS_DIR = PROJECT_ROOT / "lib" / "meshtastic_customizations"
BACKUP_DIR = CUSTOMIZATIONS_DIR / "upstream_backup"

def backup_current_library():
    """Backup current Meshtastic library before updating"""
    print("📦 Backing up current Meshtastic library...")
    if BACKUP_DIR.exists():
        shutil.rmtree(BACKUP_DIR)
    shutil.copytree(MESHTASTIC_LIB, BACKUP_DIR)
    print(f"✅ Backup created at {BACKUP_DIR}")

def download_latest_meshtastic():
    """Download latest Meshtastic-arduino from GitHub"""
    print("⬇️  Downloading latest Meshtastic-arduino...")

    with tempfile.TemporaryDirectory() as temp_dir:
        zip_path = Path(temp_dir) / "meshtastic.zip"

        # Download latest release
        urlretrieve(f"{MESHTASTIC_REPO}/archive/refs/heads/master.zip", zip_path)

        # Extract
        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            zip_ref.extractall(temp_dir)

        # Find extracted folder
        extracted_folder = Path(temp_dir) / "Meshtastic-arduino-master" / "src"

        if not extracted_folder.exists():
            raise Exception("Could not find src folder in downloaded archive")

        return extracted_folder

def update_protobuf_files(source_dir):
    """Update protobuf and core files while preserving customizations"""
    print("🔄 Updating protobuf definitions...")

    # Files to update from upstream
    files_to_update = [
        "*.pb.h", "*.pb.c",  # All protobuf files
        "pb.h", "pb_*.h", "pb_*.c",  # Protobuf library files
        "mt_internals.h", "mt_protocol.cpp",  # Core protocol files
        "Meshtastic.h"  # Main header
    ]

    # Files to preserve (our ESP32 customizations)
    esp32_customizations = {
        "mt_serial.cpp": CUSTOMIZATIONS_DIR / "esp32_overrides" / "mt_serial_esp32.cpp",
        "mt_wifi.cpp": CUSTOMIZATIONS_DIR / "esp32_overrides" / "mt_wifi_esp32.cpp"
    }

    # Update protobuf and core files
    for pattern in files_to_update:
        for file_path in source_dir.glob(pattern):
            dest_path = MESHTASTIC_LIB / file_path.name
            shutil.copy2(file_path, dest_path)
            print(f"  ✅ Updated {file_path.name}")

    # Also update meshtastic subdirectory
    meshtastic_src = source_dir / "meshtastic"
    meshtastic_dest = MESHTASTIC_LIB / "meshtastic"

    if meshtastic_src.exists():
        if meshtastic_dest.exists():
            shutil.rmtree(meshtastic_dest)
        shutil.copytree(meshtastic_src, meshtastic_dest)
        print("  ✅ Updated meshtastic protobuf directory")

    # Copy our ESP32 customizations to the main library
    for dest_file, src_file in esp32_customizations.items():
        if src_file.exists():
            dest_path = MESHTASTIC_LIB / dest_file
            shutil.copy2(src_file, dest_path)
            print(f"  ✅ Applied ESP32 customization: {dest_file}")
        else:
            print(f"  ⚠️  ESP32 customization not found: {src_file}")

    # Preserve README
    readme_path = MESHTASTIC_LIB / "README YS"
    if readme_path.exists():
        print(f"  ✅ Preserved README YS")

def verify_customizations():
    """Verify that our customizations are still in place"""
    print("🔍 Verifying customizations...")

    required_files = [
        "mt_serial.cpp",
        "mt_wifi.cpp"
    ]

    for file in required_files:
        file_path = MESHTASTIC_LIB / file
        if not file_path.exists():
            print(f"  ❌ Missing customization: {file}")
            return False

        # Check if it contains ESP32-specific code
        with open(file_path, 'r') as f:
            content = f.read()

        if "HardwareSerial(2)" in content or "ESP32" in content:
            print(f"  ✅ Found ESP32 customization: {file}")
        else:
            print(f"  ⚠️  File exists but may not be ESP32 customized: {file}")

    return True

def update_platformio_config():
    """Update platformio.ini to use new structure if needed"""
    print("⚙️  Checking platformio.ini configuration...")

    platformio_ini = PROJECT_ROOT / "platformio.ini"
    if platformio_ini.exists():
        with open(platformio_ini, 'r') as f:
            content = f.read()

        # Check if we need to add our customizations directory
        if "lib/meshtastic_customizations" not in content:
            print("  ℹ️  Consider adding 'lib/meshtastic_customizations/esp32_overrides' to lib_extra_dirs")
        else:
            print("  ✅ platformio.ini already configured")

def main():
    """Main update process"""
    print("🚀 Starting Meshtastic update process for Golf Cart Project")
    print("=" * 60)

    try:
        # Step 1: Backup current library
        backup_current_library()

        # Step 2: Download latest Meshtastic
        source_dir = download_latest_meshtastic()

        # Step 3: Update files while preserving customizations
        update_protobuf_files(source_dir)

        # Step 4: Verify customizations
        if not verify_customizations():
            print("❌ Customization verification failed!")
            return False

        # Step 5: Check platformio config
        update_platformio_config()

        print("=" * 60)
        print("✅ Meshtastic update completed successfully!")
        print("📋 Summary:")
        print("  - Protobuf definitions updated to latest")
        print("  - ESP32 customizations preserved")
        print("  - Backup available in upstream_backup/")
        print("\n💡 Next steps:")
        print("  1. Test compile your project")
        print("  2. Test Meshtastic communication")
        print("  3. If issues occur, customizations are in esp32_overrides/")

        return True

    except Exception as e:
        print(f"❌ Update failed: {e}")
        print("🔧 Restoring from backup...")
        if BACKUP_DIR.exists():
            shutil.rmtree(MESHTASTIC_LIB)
            shutil.copytree(BACKUP_DIR, MESHTASTIC_LIB)
            print("✅ Backup restored")
        return False

if __name__ == "__main__":
    main()