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

// GCI deep-sleep DM persistence (up to 4 unread DMs survive a sleep/wake cycle)
void saveDmsToNvs(void);
void loadDmsFromNvs(void);

#endif // PREFERENCES_MANAGER_H