/********************************************************************************************
*    GCD Tone Tester - Standalone program to test and tune alert tones                    *
*                                                                                           *
*    Usage:                                                                                 *
*    1. Comment out main.cpp from platformio.ini build                                      *
*    2. Add this file to platformio.ini: build_src_filter = +<../test/tone_tester.cpp>     *
*    3. Upload and connect serial monitor                                                   *
*    4. Press number keys to test different tones                                           *
*    5. Edit tone parameters below and re-upload to tune                                    *
*                                                                                           *
********************************************************************************************/

#include <Arduino.h>

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
    const char* description;
};

TonePattern tones[] = {
    {"STARTUP",    1, 2500, 100, 200, "Single mid-high beep - system ready"},
    {"ALERT",      3, 3000, 150, 100, "Triple urgent beeps - attention required"},
    {"MESSAGE",    2, 2000,  80, 120, "Double soft beeps - message received"},
    {"SLEEP",      1, 1500, 200,   0, "Single low beep - entering sleep"},
    {"ERROR",      4, 3500, 100, 100, "Quad rapid high beeps - error condition"},
    {"CONFIRM",    1, 2200,  50,   0, "Quick chirp - action confirmed"},
    {"WARNING",    2, 2800, 120, 150, "Double medium beeps - warning"},
    {"CANCEL",     1, 1800, 150,   0, "Single low-mid beep - action cancelled"}
};

const int NUM_TONES = sizeof(tones) / sizeof(TonePattern);

// Timer-based beep system (non-blocking)
static int beepCount = 0;
static int targetBeeps = 0;
static bool beepOn = false;
static hw_timer_t *beepTimer = NULL;
static uint32_t currentBeepFrequency = BEEP_FREQUENCY_HZ;
static uint32_t currentBeepDuration = 100;
static uint32_t currentBeepPause = 200;

void IRAM_ATTR beepTimerISR() {
    if (beepOn) {
        // Turn off beep
        ledcWrite(SPEAKER_LEDC_CHANNEL, 0);
        beepOn = false;
        beepCount++;

        if (beepCount < targetBeeps) {
            // Schedule next beep after pause
            timerAlarmWrite(beepTimer, currentBeepPause * 1000, false);
            timerAlarmEnable(beepTimer);
        } else {
            // All beeps completed
            beepCount = 0;
            targetBeeps = 0;
        }
    } else {
        // Turn on beep if we haven't reached target count
        if (beepCount < targetBeeps) {
            if (speaker_volume <= 0) {
                ledcWrite(SPEAKER_LEDC_CHANNEL, 0);
            } else {
                uint32_t maxDuty = (1 << SPEAKER_LEDC_TIMER_BIT) / 2;
                uint32_t volumeDuty = (maxDuty * speaker_volume) / 100;
                ledcWrite(SPEAKER_LEDC_CHANNEL, volumeDuty);
            }
            beepOn = true;

            // Schedule beep off after beep duration
            timerAlarmWrite(beepTimer, currentBeepDuration * 1000, false);
            timerAlarmEnable(beepTimer);
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

    // Create hardware timer for beeps (1 MHz = 1 microsecond resolution)
    beepTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(beepTimer, &beepTimerISR, true);

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

    // Update timing parameters
    currentBeepDuration = duration;
    currentBeepPause = pauseMs;

    // Stop any ongoing beep sequence
    timerAlarmDisable(beepTimer);
    ledcWrite(SPEAKER_LEDC_CHANNEL, 0);

    // Reset counters and start new sequence
    beepCount = 0;
    targetBeeps = numBeeps;
    beepOn = false;

    // Start the beep sequence
    beepTimerISR();
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
    Serial.println("========================================");

    beep(t.numBeeps, t.frequency, t.duration, t.pause);
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
    Serial.println("═══════════════════════════════════════════════════════════════════════════");
    Serial.printf("%-12s %6s %9s %10s %8s\n", "Name", "Beeps", "Freq(Hz)", "Dur(ms)", "Pause(ms)");
    Serial.println("───────────────────────────────────────────────────────────────────────────");

    for (int i = 0; i < NUM_TONES; i++) {
        TonePattern &t = tones[i];
        Serial.printf("%-12s %6d %9d %10d %8d\n",
                      t.name, t.numBeeps, t.frequency, t.duration, t.pause);
    }
    Serial.println("═══════════════════════════════════════════════════════════════════════════\n");
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
    Serial.begin(115200);
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
