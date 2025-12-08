# Meshtastic Library Patches

This directory contains critical bug fixes for the upstream Meshtastic-arduino library that are required for the Golf Cart Display project to function correctly.

## Why Patches Are Needed

The upstream Meshtastic-arduino library has bugs that cause issues in our specific use case. Rather than maintaining a fork or manually editing the library files (which would be lost on updates), we use an automated patch system to apply these fixes.

## Patch Management

### Automatic Application
Patches are automatically applied when running the update script:
```bash
cd lib/meshtastic_customizations/update_scripts/
python3 update_meshtastic.py
```

The update script will:
1. Download latest upstream code
2. Apply all patches automatically via `apply_patches.py`
3. Verify the patches were applied successfully

### Manual Application
If you've manually updated the library, apply patches with:
```bash
cd lib/meshtastic_customizations/patches/
python3 apply_patches.py
```

## Current Patches

### 1. Rebooted Tag Return Statement Fix
**File**: `mt_protocol_rebooted_tag_fix.patch`
**Affected File**: `lib/meshtastic-arduino_src/mt_protocol.cpp`
**Line**: 686
**Upstream Status**: Bug exists in upstream as of 2025-12
**GitHub Issue**: https://github.com/meshtastic/Meshtastic-arduino (bug not yet reported)

#### Problem
Missing `return true;` statement after the `rebooted_tag` case (tag 8) in the FromRadio message handler switch statement causes fall-through to the `moduleConfig_tag` case. All other cases in the switch statement properly return a boolean value; this one was missing the return statement.

#### Symptom
```
Guru Meditation Error: Core 1 panic'ed (InstrFetchProhibited). Exception was unhandled.
PC: 0x00000000
```

Occurs when:
- GCD (Golf Cart Display) remains running
- GCM (Golf Cart Mesh radio) is power-cycled
- GCM reconnects and sends `rebooted_tag` message
- Code falls through to `moduleConfig_tag` with invalid data
- Null pointer dereference causes crash

#### Solution
Add `return true;` statement after line 685 (consistent with all other cases):
```cpp
case meshtastic_FromRadio_rebooted_tag: // 8
  Serial.println("*** Received rebooted_tag! ***");
  handleGcmRebooted();  // Notify callback that GCM has rebooted
  _mt_send_toRadio(toRadio);
  return true;  // Fix upstream bug: prevent fall-through to moduleConfig_tag

case meshtastic_FromRadio_moduleConfig_tag: // 9
  return handle_moduleConfig_tag(&fromRadio.moduleConfig);
```

#### Testing
After applying patch:
1. Boot GCD with GCM connected → wake notification should send
2. Power-cycle GCM while GCD stays running
3. When GCM reconnects → should send `rebooted_tag`
4. `handleGcmRebooted()` should run without crash
5. Wake notification should be resent

## Adding New Patches

If you discover a new upstream bug that needs fixing:

1. **Document the bug**: Create a `.patch` file with the fix
2. **Update `apply_patches.py`**: Add a new function to apply the patch
3. **Update this README**: Document what the patch fixes and why
4. **Update CLAUDE.md**: Add to the "Current Applied Patches" list
5. **Test**: Verify patch applies cleanly to fresh upstream code

### Patch File Format
```patch
--- original_file.cpp.orig	2025-01-XX XX:XX:XX
+++ original_file.cpp	2025-01-XX XX:XX:XX
@@ -line,count +line,count @@
 context line
-removed line
+added line
 context line
```

## Verification

After applying patches, verify with:
```bash
# Check if patch was applied
grep -n "Fix upstream bug" lib/meshtastic-arduino_src/mt_protocol.cpp

# Expected output:
# 686:      return true;  // Fix upstream bug: prevent fall-through to moduleConfig_tag
```

## Upstream Bug Reporting

When an upstream bug is fixed:
1. Update this README to note the fix version
2. Keep the patch for backward compatibility with older versions
3. Eventually remove the patch when minimum supported upstream version includes the fix

## Troubleshooting

### Patch Won't Apply
If `apply_patches.py` reports that a patch can't be applied:
1. Check if upstream already fixed the bug
2. Compare patch against current upstream code
3. Update patch file if needed
4. Report discrepancy in project issues

### Build Errors After Patching
1. Review the patch output for warnings
2. Check if upstream changed the affected code structure
3. Manually verify the fix was applied at the correct location
4. Update patch if upstream refactored the code
