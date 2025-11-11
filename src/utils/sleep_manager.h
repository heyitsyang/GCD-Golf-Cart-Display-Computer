#ifndef SLEEP_MANAGER_H
#define SLEEP_MANAGER_H

#include <Arduino.h>

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

#endif // SLEEP_MANAGER_H
