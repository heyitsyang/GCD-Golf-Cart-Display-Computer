#ifndef SLEEP_MANAGER_H
#define SLEEP_MANAGER_H

#include <Arduino.h>

/**
 * Early sleep pin check - call before any display initialization
 * If SLEEP_PIN is LOW, immediately returns to deep sleep
 */
void checkSleepPinEarly();

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
