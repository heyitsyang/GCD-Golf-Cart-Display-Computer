#include "preferences_manager.h"
#include "config.h"
#include "globals.h"
#include "types.h"

void initPreferences() {
    prefs.begin("eeprom", false);
    Serial.println("Preferences initialized");

    // Check NVS stats
    size_t usedEntries = prefs.freeEntries();
    Serial.printf("NVS Free Entries: %d\n", usedEntries);
}

void loadPreferences() {
    day_backlight = prefs.getInt("day_backlight", 10);
    Serial.print("> day_backlight read from eeprom = ");
    Serial.println(day_backlight);
    old_day_backlight = day_backlight;

    night_backlight = prefs.getInt("night_backlight", 5);
    Serial.print("> night_backlight read from eeprom = ");
    Serial.println(night_backlight);
    old_night_backlight = night_backlight;

    // Key name must be 15 chars or less for NVS - use "gci_mac" (7 chars)
    espnow_gci_mac_addr = prefs.getString("gci_mac", "NONE");
    Serial.print("> espnow_gci_mac_addr read from eeprom = ");
    Serial.println(espnow_gci_mac_addr);
    old_espnow_gci_mac_addr = espnow_gci_mac_addr;

    flip_screen = prefs.getBool("flip_screen", false);
    Serial.print("> flip_screen read from eeprom = ");
    Serial.println(flip_screen ? "true" : "false");
    set_var_flip_screen(flip_screen);
    old_flip_screen = flip_screen;

    speaker_volume = prefs.getInt("speaker_volume", 10);
    Serial.print("> speaker_volume read from eeprom = ");
    Serial.println(speaker_volume);
    set_var_speaker_volume(speaker_volume);
    old_speaker_volume = speaker_volume;

    accumDistance = prefs.getFloat("accumDistance", 0.0);
    Serial.print("> accumDistance read from eeprom = ");
    Serial.println(accumDistance, 3);
    odometer = String(accumDistance, 1);
    set_var_odometer(odometer.c_str());

    tripDistance = prefs.getFloat("tripDistance", 0.0);
    Serial.print("> tripDistance read from eeprom = ");
    Serial.println(tripDistance, 3);
    trip_odometer = String(tripDistance, 1);
    set_var_trip_odometer(trip_odometer.c_str());

    // hrs_since_svc is stored as TENTHS of hours everywhere (0.1 hr = 6 min resolution)
    // Load directly without calling setter to avoid premature EEPROM queue write
    hrs_since_svc = prefs.getInt("hrs_since_svc", 0);
    Serial.print("> hrs_since_svc read from eeprom (tenths) = ");
    Serial.println(hrs_since_svc);
    Serial.print(">   (Display value in hours: ");
    Serial.print(hrs_since_svc / 10);
    Serial.println(")");

    svc_interval_hrs = prefs.getInt("svc_interval_hrs", 100);
    Serial.print("> svc_interval_hrs read from eeprom = ");
    Serial.println(svc_interval_hrs);
    set_var_svc_interval_hrs(svc_interval_hrs);
    old_svc_interval_hrs = svc_interval_hrs;

    temperature_adj = prefs.getFloat("temperature_adj", 0.0);
    Serial.print("> temperature_adj read from eeprom = ");
    Serial.println(temperature_adj);
    set_var_temperature_adj(temperature_adj);
    old_temperature_adj = temperature_adj;

    // Load touchscreen calibration coefficients if available
    touch_alpha_x = prefs.getFloat("touch_alpha_x", 0.0);
    touch_beta_x = prefs.getFloat("touch_beta_x", 0.0);
    touch_delta_x = prefs.getFloat("touch_delta_x", 0.0);
    touch_alpha_y = prefs.getFloat("touch_alpha_y", 0.0);
    touch_beta_y = prefs.getFloat("touch_beta_y", 0.0);
    touch_delta_y = prefs.getFloat("touch_delta_y", 0.0);

    // Check if calibration coefficients are valid (not all zeros)
    if (touch_alpha_x != 0.0 || touch_beta_x != 0.0 || touch_alpha_y != 0.0 || touch_beta_y != 0.0) {
        use_touch_calibration = true;
        Serial.println("> Touchscreen calibration coefficients loaded from EEPROM:");
        Serial.print("  alpha_x = "); Serial.println(touch_alpha_x, 6);
        Serial.print("  beta_x = "); Serial.println(touch_beta_x, 6);
        Serial.print("  delta_x = "); Serial.println(touch_delta_x, 6);
        Serial.print("  alpha_y = "); Serial.println(touch_alpha_y, 6);
        Serial.print("  beta_y = "); Serial.println(touch_beta_y, 6);
        Serial.print("  delta_y = "); Serial.println(touch_delta_y, 6);
    } else {
        use_touch_calibration = false;
        Serial.println("> No touchscreen calibration found in EEPROM, using default auto-calibration");
    }
}

void queuePreferenceWrite(const char* key, float value) {
    eepromWriteItem_t item;
    item.type = EEPROM_FLOAT;
    strcpy(item.key, key);
    item.value.floatVal = value;
    xQueueSend(eepromWriteQueue, &item, 0);
}

void queuePreferenceWrite(const char* key, int value) {
    eepromWriteItem_t item;
    item.type = EEPROM_INT;
    strcpy(item.key, key);
    item.value.intVal = value;
    xQueueSend(eepromWriteQueue, &item, 0);
}

void queuePreferenceWrite(const char* key, const String& value) {
    eepromWriteItem_t item;
    item.type = EEPROM_STRING;
    strcpy(item.key, key);
    strcpy(item.value.stringVal, value.c_str());
    xQueueSend(eepromWriteQueue, &item, 0);
}

void queuePreferenceWrite(const char* key, bool value) {
    eepromWriteItem_t item;
    item.type = EEPROM_BOOL;
    strcpy(item.key, key);
    item.value.boolVal = value;
    xQueueSend(eepromWriteQueue, &item, 0);
}

void clearAllPreferences() {
    Serial.println("Clearing all EEPROM preferences...");
    prefs.clear();
    Serial.println("All preferences cleared. Restarting system...");
    ESP.restart();
}