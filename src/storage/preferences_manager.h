#ifndef PREFERENCES_MANAGER_H
#define PREFERENCES_MANAGER_H

#include <Arduino.h>

void initPreferences();
void loadPreferences();
void clearAllPreferences();
void queuePreferenceWrite(const char* key, float value);
void queuePreferenceWrite(const char* key, int value);
void queuePreferenceWrite(const char* key, const String& value);
void queuePreferenceWrite(const char* key, bool value);

#endif // PREFERENCES_MANAGER_H