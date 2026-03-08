#ifndef SLEEP_MANAGER_H
#define SLEEP_MANAGER_H

#include <Arduino.h>

/**
 * Check for spurious wake and return to deep sleep if SLEEP_PIN is already LOW.
 * Call this as early as possible in setup(), before any display or peripheral init.
 * If the device was woken by EXT0 (SLEEP_PIN) but the pin is already LOW after a
 * short settle time, the wake was a glitch and the device goes back to deep sleep.
 */
void checkSpuriousWake();

/**
 * Initialize sleep pin configuration
 * Call this from setup()
 */
void initSleepPin();

/**
 * Check if the device should enter sleep mode
 * Returns true if SLEEP_PIN is LOW
 */
bool shouldEnterSleep();

/**
 * Enter deep sleep mode
 * Device will reboot from setup() when SLEEP_PIN goes HIGH
 * Maximum power savings (~10uA)
 * All RAM is lost, device performs full restart on wake
 */
void enterDeepSleep();

/**
 * Initialize the sleep mode state machine
 * Call this from setup() after loadPreferences()
 */
void initSleepModeStateMachine();

/**
 * Process the sleep mode state machine
 * Call this from system_task main loop
 * Returns true if deep sleep should be entered
 */
bool processSleepModeStateMachine();

#endif // SLEEP_MANAGER_H
