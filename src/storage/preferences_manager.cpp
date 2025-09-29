#include "preferences_manager.h"
#include "config.h"
#include "globals.h"
#include "types.h"

void initPreferences() {
    prefs.begin("eeprom", false);
    Serial.println("Preferences initialized");
}

void loadPreferences() {
    max_hdop = prefs.getFloat("max_hdop", 2.5);
    Serial.print("> max_hdop read from eeprom = ");
    Serial.println(max_hdop);
    old_max_hdop = max_hdop;
    
    day_backlight = prefs.getInt("day_backlight", 10);
    Serial.print("> day_backlight read from eeprom = ");
    Serial.println(day_backlight);
    old_day_backlight = day_backlight;
    
    night_backlight = prefs.getInt("night_backlight", 5);
    Serial.print("> night_backlight read from eeprom = ");
    Serial.println(night_backlight);
    old_night_backlight = night_backlight;
    
    espnow_mac_addr = prefs.getString("espnow_mac_addr", "NONE");
    Serial.print("> espnow_mac_addr read from eeprom = ");
    Serial.println(espnow_mac_addr);
    old_espnow_mac_addr = espnow_mac_addr;

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