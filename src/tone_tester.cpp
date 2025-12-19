/********************************************************************************************
*    GCD Tone Tester - Standalone program to test and tune alert tones                    *
*                                                                                           *
*    Usage:                                                                                 *
*    Build and upload: pio run -e tone_test --target upload && pio device monitor -e tone_test
*    Press number keys to test different tones                                              *
*    Edit tone parameters below and re-upload to tune                                       *
*                                                                                           *
********************************************************************************************/

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

// Hardware configuration
#define SPEAKER_PIN 26
#define SPEAKER_LEDC_CHANNEL 1
#define SPEAKER_LEDC_TIMER_BIT 8
#define BEEP_FREQUENCY_HZ 2500

// Tone volume (0-100%)
int speaker_volume = 20;

// Tone definitions - EDIT THESE TO TUNE YOUR TONES
struct TonePattern {
    const char* name;
    int numBeeps;
    uint32_t frequency;
    uint32_t duration;
    uint32_t pause;
    int volumePercent;  // Relative volume 0-100% (multiplied with speaker_volume)
    const char* description;
};

TonePattern tones[] = {
    // name,   beeps, freq, dur, pause, vol%, description
    {"STARTUP",    1, 2500,  40,   0,  70,    "Single mid-high beep - system ready"},
    {"SLEEP",      1, 2500, 500,   0,  50,    "Single long mid-high beep - entering sleep"},
    {"MESSAGE",    2, 2500, 100, 100,  80,    "Double soft beeps - message received"},
    {"ALERT",      1, 1500, 250, 100,  50,    "One long mid beep - reminder warning"},
    {"URGENT",     4, 1500, 200, 100,  80,    "Quad rapid mid beeps - urgent attention required"},
    {"CONFIRM",    1, 2200,  50,   0,  60,    "Quick chirp - action confirmed"},
    {"CLICK",      1,  20,   51,   0,  60,    "Click - haptic feedback"},
    {"ERROR",      1,  90,  150,   0,  10,    "Single low beep - action not allowed"}
};

const int NUM_TONES = sizeof(tones) / sizeof(TonePattern);
// Beep control variables
static int beepCount = 0;
static int targetBeeps = 0;
static bool beepOn = false;
static TimerHandle_t beepTimer = NULL;
static uint32_t currentBeepFrequency = BEEP_FREQUENCY_HZ;
static uint32_t currentBeepDuration = 100;
static uint32_t currentBeepPause = 200;
static int32_t currentBeepVolume = 20;  // Local copy for timer callback

// Beep timer callback function
void beepTimerCallback(TimerHandle_t xTimer) {
    if (beepOn) {
        // Turn off beep
        ledcWrite(SPEAKER_LEDC_CHANNEL, 0);
        beepOn = false;
        beepCount++;

        if (beepCount < targetBeeps) {
            // Schedule next beep after pause
            xTimerChangePeriod(beepTimer, pdMS_TO_TICKS(currentBeepPause), 0);
        } else {
            // All beeps completed, stop timer
            xTimerStop(beepTimer, 0);
            beepCount = 0;
            targetBeeps = 0;
        }
    } else {
        // Turn on beep if we haven't reached target count
        if (beepCount < targetBeeps) {
            // Check volume - mute if volume is 0
            if (currentBeepVolume <= 0) {
                ledcWrite(SPEAKER_LEDC_CHANNEL, 0);
            } else {
                // Calculate duty cycle based on volume (0-100% -> 0%-100%)
                uint32_t maxDuty = (1 << SPEAKER_LEDC_TIMER_BIT) / 2; // 50% max duty cycle
                uint32_t volumeDuty = (maxDuty * currentBeepVolume) / 100;
                ledcWrite(SPEAKER_LEDC_CHANNEL, volumeDuty);
            }
            beepOn = true;

            // Schedule beep off after beep duration
            xTimerChangePeriod(beepTimer, pdMS_TO_TICKS(currentBeepDuration), 0);
        }
    }
}

void initSpeaker() {
    // Initialize LEDC for speaker
#if ESP_IDF_VERSION_MAJOR == 5
    ledcAttach(SPEAKER_PIN, BEEP_FREQUENCY_HZ, SPEAKER_LEDC_TIMER_BIT);
#else
    ledcSetup(SPEAKER_LEDC_CHANNEL, BEEP_FREQUENCY_HZ, SPEAKER_LEDC_TIMER_BIT);
    ledcAttachPin(SPEAKER_PIN, SPEAKER_LEDC_CHANNEL);
#endif

    // Create FreeRTOS timer for beeps
    beepTimer = xTimerCreate("BeepTimer", pdMS_TO_TICKS(100),
                             pdFALSE, NULL, beepTimerCallback);

    Serial.println("Speaker initialized");
}

void beep(int numBeeps, uint32_t frequency, uint32_t duration, uint32_t pauseMs) {
    if (numBeeps <= 0 || beepTimer == NULL) {
        return;
    }

    // Update frequency if different
    if (frequency != currentBeepFrequency) {
        currentBeepFrequency = frequency;
#if ESP_IDF_VERSION_MAJOR == 5
        ledcChangeFrequency(SPEAKER_PIN, frequency, SPEAKER_LEDC_TIMER_BIT);
#else
        ledcSetup(SPEAKER_LEDC_CHANNEL, frequency, SPEAKER_LEDC_TIMER_BIT);
#endif
    }

    // Update timing parameters and copy current volume for timer callback
    currentBeepDuration = duration;
    currentBeepPause = pauseMs;
    currentBeepVolume = speaker_volume;  // Safe copy for timer callback

    // Stop any ongoing beep sequence
    xTimerStop(beepTimer, 0);
    ledcWrite(SPEAKER_LEDC_CHANNEL, 0);

    // Reset counters and start new sequence
    beepCount = 0;
    targetBeeps = numBeeps;
    beepOn = false;

    // Start the beep sequence
    beepTimerCallback(beepTimer);
}

void playTone(int index) {
    if (index < 0 || index >= NUM_TONES) {
        Serial.println("Invalid tone index!");
        return;
    }

    TonePattern &t = tones[index];
    Serial.println("\n========================================");
    Serial.printf("Playing: %s\n", t.name);
    Serial.printf("  %s\n", t.description);
    Serial.printf("  Beeps: %d | Freq: %d Hz | Duration: %d ms | Pause: %d ms\n",
                  t.numBeeps, t.frequency, t.duration, t.pause);
    Serial.printf("  Volume: %d%% (effective: %d%%)\n",
                  t.volumePercent, (speaker_volume * t.volumePercent) / 100);
    Serial.println("========================================");

    // Calculate effective volume (speaker_volume * tone's volumePercent)
    int savedVolume = speaker_volume;
    speaker_volume = (speaker_volume * t.volumePercent) / 100;

    beep(t.numBeeps, t.frequency, t.duration, t.pause);

    // Restore original volume
    speaker_volume = savedVolume;
}

void printMenu() {
    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════════════════════════════╗");
    Serial.println("║              GCD TONE TESTER - Audio Tuning Tool              ║");
    Serial.println("╚════════════════════════════════════════════════════════════════╝");
    Serial.println();
    Serial.println("Available Tones:");
    Serial.println("────────────────────────────────────────────────────────────────");

    for (int i = 0; i < NUM_TONES; i++) {
        Serial.printf("  [%d] %-12s - %s\n", i + 1, tones[i].name, tones[i].description);
    }

    Serial.println("────────────────────────────────────────────────────────────────");
    Serial.println("  [V] Adjust volume (current: " + String(speaker_volume) + "%)");
    Serial.println("  [M] Show this menu");
    Serial.println("  [I] Show tone details");
    Serial.println("────────────────────────────────────────────────────────────────");
    Serial.println("\nPress a number (1-" + String(NUM_TONES) + ") to play a tone...\n");
}

void printToneDetails() {
    Serial.println("\n\nDetailed Tone Parameters:");
    Serial.println("═════════════════════════════════════════════════════════════════════════════════");
    Serial.printf("%-12s %6s %9s %10s %8s %6s %9s\n",
                  "Name", "Beeps", "Freq(Hz)", "Dur(ms)", "Pause(ms)", "Vol%", "Eff.Vol%");
    Serial.println("─────────────────────────────────────────────────────────────────────────────────");

    for (int i = 0; i < NUM_TONES; i++) {
        TonePattern &t = tones[i];
        int effectiveVol = (speaker_volume * t.volumePercent) / 100;
        Serial.printf("%-12s %6d %9d %10d %8d %6d %9d\n",
                      t.name, t.numBeeps, t.frequency, t.duration, t.pause,
                      t.volumePercent, effectiveVol);
    }
    Serial.println("═════════════════════════════════════════════════════════════════════════════════\n");
}

void adjustVolume() {
    Serial.println("\n────────────────────────────────────────");
    Serial.println("Enter new volume (0-100): ");

    while (!Serial.available()) {
        delay(10);
    }

    int newVolume = Serial.parseInt();

    // Clear remaining input
    while (Serial.available()) {
        Serial.read();
    }

    if (newVolume >= 0 && newVolume <= 100) {
        speaker_volume = newVolume;
        Serial.printf("Volume set to %d%%\n", speaker_volume);

        // Play test beep
        Serial.println("Playing test beep...");
        beep(1, 2500, 100, 0);
    } else {
        Serial.println("Invalid volume! Must be 0-100");
    }
    Serial.println("────────────────────────────────────────\n");
}

void setup() {
    Serial.begin(9600);
    delay(1000);

    Serial.println("\n\n\n");
    Serial.println("╔════════════════════════════════════════════════════════════════╗");
    Serial.println("║                  GCD Tone Tester Starting...                  ║");
    Serial.println("╚════════════════════════════════════════════════════════════════╝");

    initSpeaker();

    // Play startup beep
    Serial.println("\nPlaying startup beep...");
    beep(1, 2500, 100, 200);
    delay(500);

    printMenu();
}

void loop() {
    if (Serial.available()) {
        char input = Serial.read();

        // Clear any remaining input
        while (Serial.available()) {
            Serial.read();
        }

        // Convert to uppercase
        if (input >= 'a' && input <= 'z') {
            input = input - 32;
        }

        // Handle commands
        if (input >= '1' && input <= '9') {
            int index = input - '1';
            if (index < NUM_TONES) {
                playTone(index);
            } else {
                Serial.println("Invalid tone number!");
            }
        } else if (input == 'M') {
            printMenu();
        } else if (input == 'I') {
            printToneDetails();
        } else if (input == 'V') {
            adjustVolume();
        } else if (input == '\n' || input == '\r') {
            // Ignore newlines
        } else {
            Serial.println("Unknown command: " + String(input));
            Serial.println("Press 'M' for menu");
        }
    }

    delay(10);
}
