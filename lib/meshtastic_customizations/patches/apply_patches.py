#!/usr/bin/env python3
"""
Apply required patches to Meshtastic library after updates

This script applies critical bug fixes to the upstream Meshtastic library
that are needed for the Golf Cart project to function properly.
"""

import os
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.parent.parent.parent
MESHTASTIC_LIB = PROJECT_ROOT / "lib" / "meshtastic-arduino_src"

def apply_rebooted_tag_fix():
    """
    Apply missing break statement fix to mt_protocol.cpp

    Upstream bug: Missing break after rebooted_tag case causes fall-through
    to moduleConfig_tag, leading to crashes when GCM reboots.

    Reference: meshtastic_rebooted_tag_fix.patch
    """
    print("🔧 Applying rebooted_tag break statement fix...")

    protocol_file = MESHTASTIC_LIB / "mt_protocol.cpp"

    if not protocol_file.exists():
        print(f"  ❌ File not found: {protocol_file}")
        return False

    # Read the file
    with open(protocol_file, 'r') as f:
        content = f.read()

    # Check if patch is already applied
    if "return true;  // Fix upstream bug: prevent fall-through to moduleConfig_tag" in content:
        print("  ✅ Patch already applied")
        return True

    # Apply the fix
    old_code = """    case meshtastic_FromRadio_rebooted_tag: // 8
      Serial.println("*** Received rebooted_tag! ***");
      handleGcmRebooted();  // Notify callback that GCM has rebooted
      _mt_send_toRadio(toRadio);

    case  meshtastic_FromRadio_moduleConfig_tag: // 9"""

    new_code = """    case meshtastic_FromRadio_rebooted_tag: // 8
      Serial.println("*** Received rebooted_tag! ***");
      handleGcmRebooted();  // Notify callback that GCM has rebooted
      _mt_send_toRadio(toRadio);
      return true;  // Fix upstream bug: prevent fall-through to moduleConfig_tag

    case  meshtastic_FromRadio_moduleConfig_tag: // 9"""

    if old_code in content:
        content = content.replace(old_code, new_code)

        # Write the patched file
        with open(protocol_file, 'w') as f:
            f.write(content)

        print("  ✅ Successfully applied rebooted_tag fix")
        return True
    else:
        print("  ⚠️  Code pattern not found - file may have changed")
        print("  ℹ️  Manual patch may be required - see mt_protocol_rebooted_tag_fix.patch")
        return False

def main():
    """Apply all required patches"""
    print("🚀 Applying Meshtastic patches for Golf Cart Project")
    print("=" * 60)

    success = True

    # Apply all patches
    if not apply_rebooted_tag_fix():
        success = False

    print("=" * 60)

    if success:
        print("✅ All patches applied successfully!")
    else:
        print("⚠️  Some patches failed - review output above")
        print("💡 Check patches/ directory for manual patch files")

    return success

if __name__ == "__main__":
    main()
