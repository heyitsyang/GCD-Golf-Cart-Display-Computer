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

def apply_node_report_callback_null_check():
    """
    Apply NULL check fixes for node_report_callback in mt_protocol.cpp

    Upstream bug: node_report_callback is called without NULL checks, causing
    crashes when GCM reconnects and triggers node reports when callback is NULL.

    Reference: mt_protocol_node_report_callback_null_check.patch
    """
    print("🔧 Applying node_report_callback NULL check fix...")

    protocol_file = MESHTASTIC_LIB / "mt_protocol.cpp"

    if not protocol_file.exists():
        print(f"  ❌ File not found: {protocol_file}")
        return False

    # Read the file
    with open(protocol_file, 'r') as f:
        content = f.read()

    # Check if patch is already applied
    if "if (node_report_callback != NULL) {" in content:
        print("  ✅ Patch already applied")
        return True

    # Apply the fix - three separate replacements for safety
    patches_applied = 0

    # Fix 1: handle_node_info
    old_code_1 = """  node_report_callback(&node, MT_NR_IN_PROGRESS);
  return true;"""

    new_code_1 = """  if (node_report_callback != NULL) {
    node_report_callback(&node, MT_NR_IN_PROGRESS);
  }
  return true;"""

    if old_code_1 in content:
        content = content.replace(old_code_1, new_code_1)
        patches_applied += 1

    # Fix 2: handle_config_complete_id - MT_NR_DONE
    old_code_2 = """    want_config_id = 0;
    node_report_callback(NULL, MT_NR_DONE);
    node_report_callback = NULL;"""

    new_code_2 = """    want_config_id = 0;
    if (node_report_callback != NULL) {
      node_report_callback(NULL, MT_NR_DONE);
    }
    node_report_callback = NULL;"""

    if old_code_2 in content:
        content = content.replace(old_code_2, new_code_2)
        patches_applied += 1

    # Fix 3: handle_config_complete_id - MT_NR_INVALID
    old_code_3 = """  } else {
    node_report_callback(NULL, MT_NR_INVALID);  // but return true, since it was still a valid packet
  }"""

    new_code_3 = """  } else {
    if (node_report_callback != NULL) {
      node_report_callback(NULL, MT_NR_INVALID);  // but return true, since it was still a valid packet
    }
  }"""

    if old_code_3 in content:
        content = content.replace(old_code_3, new_code_3)
        patches_applied += 1

    if patches_applied == 3:
        # Write the patched file
        with open(protocol_file, 'w') as f:
            f.write(content)

        print(f"  ✅ Successfully applied {patches_applied} NULL check fixes")
        return True
    elif patches_applied > 0:
        print(f"  ⚠️  Only applied {patches_applied}/3 fixes - file may have changed")
        return False
    else:
        print("  ⚠️  Code patterns not found - file may have changed")
        print("  ℹ️  Manual patch may be required - see mt_protocol_node_report_callback_null_check.patch")
        return False

def apply_random_id_fix():
    """
    Replace random() with esp_random() for packet IDs in mt_send_text()

    Upstream bug: Uses Arduino random() which produces deterministic sequences
    on ESP32 (randomSeed() has no effect). This causes identical packet IDs
    across reboots, which triggers Meshtastic's duplicate detection on the
    receiving radio, silently dropping all OTA transmissions.

    Fix: Use esp_random() which reads from the ESP32 hardware true RNG.

    Reference: mt_protocol_random_id_fix.patch
    """
    print("🔧 Applying random() → esp_random() packet ID fix...")

    protocol_file = MESHTASTIC_LIB / "mt_protocol.cpp"

    if not protocol_file.exists():
        print(f"  ❌ File not found: {protocol_file}")
        return False

    with open(protocol_file, 'r') as f:
        content = f.read()

    # Check if patch is already applied
    if "meshPacket.id = esp_random();" in content:
        print("  ✅ Patch already applied")
        return True

    old_code = "meshPacket.id = random(0x7FFFFFFF);"
    new_code = "meshPacket.id = esp_random();  // Fix: use hardware RNG for unique IDs across reboots"

    if old_code in content:
        content = content.replace(old_code, new_code)
        with open(protocol_file, 'w') as f:
            f.write(content)
        print("  ✅ Successfully applied random ID fix")
        return True
    else:
        print("  ⚠️  Code pattern not found - file may have changed")
        print("  ℹ️  Manual patch may be required - see mt_protocol_random_id_fix.patch")
        return False


def apply_fromradio_tag_descriptions():
    """
    Replace numeric-only FromRadio tag debug output with human-readable descriptions.

    Enhancement: The upstream debug print only shows "FromRadio tag: N" which requires
    memorizing tag numbers. This patch adds plain-English descriptions like
    "FromRadio tag: 2 (received message)" for easier serial debugging.

    Reference: mt_protocol_fromradio_tag_descriptions.patch
    """
    print("🔧 Applying FromRadio tag description enhancement...")

    protocol_file = MESHTASTIC_LIB / "mt_protocol.cpp"

    if not protocol_file.exists():
        print(f"  ❌ File not found: {protocol_file}")
        return False

    with open(protocol_file, 'r') as f:
        content = f.read()

    # Check if patch is already applied
    if 'static const char* const fromRadioTagDesc[]' in content:
        print("  ✅ Patch already applied")
        return True

    old_code = '''#if DEBUG_MESHTASTIC_CONNECTION
  Serial.printf("FromRadio tag: %d\\n", fromRadio.which_payload_variant);
#endif'''

    new_code = '''#if DEBUG_MESHTASTIC_CONNECTION
  {
    static const char* const fromRadioTagDesc[] = {
      "unknown",                    // 0
      "request ID",                 // 1
      "received message",           // 2
      "GCM info about itself",      // 3
      "info about another node",    // 4
      "radio/device config",        // 5
      "GCM log entry",              // 6
      "config handshake complete",  // 7
      "GCM just rebooted",         // 8
      "module config",              // 9
      "channel config",             // 10
      "TX queue status",            // 11
      "file transfer packet",       // 12
      "GCM hardware/firmware info", // 13
      "MQTT proxy message",         // 14
      "file info",                  // 15
    };
    uint32_t t = fromRadio.which_payload_variant;
    const char* desc = (t < sizeof(fromRadioTagDesc)/sizeof(fromRadioTagDesc[0])) ? fromRadioTagDesc[t] : "unknown";
    Serial.printf("FromRadio tag: %d (%s)\\n", t, desc);
  }
#endif'''

    if old_code in content:
        content = content.replace(old_code, new_code)
        with open(protocol_file, 'w') as f:
            f.write(content)
        print("  ✅ Successfully applied FromRadio tag descriptions")
        return True
    else:
        print("  ⚠️  Code pattern not found - file may have changed")
        print("  ℹ️  Manual patch may be required - see mt_protocol_fromradio_tag_descriptions.patch")
        return False


def main():
    """Apply all required patches"""
    print("🚀 Applying Meshtastic patches for Golf Cart Project")
    print("=" * 60)

    success = True

    # Apply all patches
    if not apply_rebooted_tag_fix():
        success = False

    if not apply_node_report_callback_null_check():
        success = False

    if not apply_random_id_fix():
        success = False

    if not apply_fromradio_tag_descriptions():
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
