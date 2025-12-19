# GCD Tone Tester

A standalone testing program for auditing and fine-tuning alert tones on the Golf Cart Display hardware.

## Purpose

This tool allows you to:
- Test different tone patterns on actual hardware
- Audition all predefined tones (Startup, Alert, Message, Sleep, Error, etc.)
- Adjust frequency, duration, and timing parameters
- Fine-tune volume levels
- Compare different tone characteristics

## How to Use

### 1. Build and Upload the Tone Tester

The tone tester has its own dedicated build environment (`tone_test`) in `platformio.ini`, similar to the calibration environment.

```bash
# Build, upload, and monitor in one command
pio run -e tone_test --target upload && pio device monitor -e tone_test

# Or separately:
pio run -e tone_test --target upload
pio device monitor -e tone_test
```

No need to modify `platformio.ini` - just specify the environment!

### 2. Interactive Menu

Once running, you'll see a menu like this:

```
╔════════════════════════════════════════════════════════════════╗
║              GCD TONE TESTER - Audio Tuning Tool              ║
╚════════════════════════════════════════════════════════════════╝

Available Tones:
────────────────────────────────────────────────────────────────
  [1] STARTUP      - Single mid-high beep - system ready
  [2] ALERT        - Triple urgent beeps - attention required
  [3] MESSAGE      - Double soft beeps - message received
  [4] SLEEP        - Single low beep - entering sleep
  [5] ERROR        - Quad rapid high beeps - error condition
  [6] CONFIRM      - Quick chirp - action confirmed
  [7] WARNING      - Double medium beeps - warning
  [8] CANCEL       - Single low-mid beep - action cancelled
────────────────────────────────────────────────────────────────
  [V] Adjust volume (current: 20%)
  [M] Show this menu
  [I] Show tone details
────────────────────────────────────────────────────────────────

Press a number (1-8) to play a tone...
```

### 3. Test Tones

- Press **1-8** to play different tones
- Press **V** to adjust volume (0-100%)
- Press **I** to see detailed tone parameters
- Press **M** to redisplay the menu

### 4. Tune the Tones

Edit the `TonePattern tones[]` array in [src/tone_tester.cpp](../src/tone_tester.cpp) to adjust:

```cpp
TonePattern tones[] = {
    // name,     beeps, freq(Hz), duration(ms), pause(ms), description
    {"STARTUP",    1,    2500,     100,          200,       "Single mid-high beep"},
    {"ALERT",      3,    3000,     150,          100,       "Triple urgent beeps"},
    // ... edit these values and re-upload to test
};
```

**Parameters:**
- **numBeeps**: How many beeps in the sequence (1-10)
- **frequency**: Pitch in Hertz (500-5000 Hz typical, 2000-3000 Hz sounds best on CYD speaker)
- **duration**: How long each beep lasts in milliseconds
- **pause**: Gap between beeps in milliseconds (0 for single beeps)

### 5. Return to Main Application

When done testing, simply build and upload the normal GCD environment:

```bash
pio run -e gcd --target upload
```

No changes to `platformio.ini` needed!

## Tuning Tips

### Frequency Guidelines
- **500-1000 Hz**: Very low, rumbling - hard to hear on small speaker
- **1200-1500 Hz**: Low, calming - good for "sleep" or "cancel" tones
- **2000-2500 Hz**: Medium, clear - good general-purpose range
- **2800-3200 Hz**: High, attention-grabbing - good for alerts
- **3500-4000 Hz**: Very high, urgent - good for errors/alarms
- **Above 4000 Hz**: Piercing - may sound harsh on CYD speaker

### Duration Guidelines
- **50-80 ms**: Quick chirp, subtle feedback
- **100-150 ms**: Standard beep, clear but not intrusive
- **200-300 ms**: Long beep, attention-demanding

### Pattern Guidelines
- **Single beep**: Confirmation, status change
- **Double beep**: Informational notification
- **Triple beep**: Warning, needs attention
- **Quad+ beeps**: Error, urgent alert

### Volume Guidelines
- **0-10%**: Very quiet, barely audible
- **10-20%**: Quiet, suitable for nighttime
- **20-40%**: Normal, general use (default: 20%)
- **40-60%**: Loud, outdoor use
- **60-100%**: Very loud, may distort on CYD speaker

## Example Tuning Session

```
# Test the MESSAGE tone
> 3
Playing: MESSAGE
  Double soft beeps - message received
  Beeps: 2 | Freq: 2000 Hz | Duration: 80 ms | Pause: 120 ms

# Hmm, too quiet. Try higher frequency and longer duration.
# Edit tone_tester.cpp:
{"MESSAGE", 2, 2400, 100, 120, "Double soft beeps - message received"},

# Re-upload and test again
> 3
Playing: MESSAGE
  ...

# Perfect! Now copy these values to your main application
```

## Applying Your Tuned Tones

Once you've finalized your tone parameters, copy them to your main GCD codebase:

1. Create wrapper functions in `src/hardware/display.h` and `display.cpp`
2. Use the tuned parameters from the tester
3. Call the tone functions where needed (message received, sleep, etc.)

See the main conversation for implementation options.
