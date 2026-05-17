#include "eeprom_task.h"
#include "config.h"
#include "globals.h"
#include "types.h"
#include "storage/preferences_manager.h"

void eepromTask(void *parameter) {
    eepromWriteItem_t item;

    while (true) {
        if (xQueueReceive(eepromWriteQueue, &item, portMAX_DELAY)) {
            if (xSemaphoreTake(eepromMutex, portMAX_DELAY)) {
                switch (item.type) {
                    case EEPROM_FLOAT: {
                        prefs.putFloat(item.key, item.value.floatVal);
#if DEBUG_EEPROM == 1
                        Serial.printf("> %s saved: %.6f\n", item.key, item.value.floatVal);
                        float readBack = prefs.getFloat(item.key, -999.0);
                        Serial.printf("  VERIFIED: %.6f\n", readBack);
#endif
                        break;
                    }

                    case EEPROM_INT:
                        prefs.putInt(item.key, item.value.intVal);
#if DEBUG_EEPROM == 1
                        Serial.printf("> %s saved: %d\n", item.key, item.value.intVal);
#endif
                        break;

                    case EEPROM_STRING: {
                        String valueToSave = String(item.value.stringVal);
                        size_t written = prefs.putString(item.key, valueToSave);
#if DEBUG_EEPROM == 1
                        Serial.printf("> %s saved: %s (%d bytes)\n", item.key, item.value.stringVal, written);
                        String readBack = prefs.getString(item.key, "VERIFY_FAILED");
                        if (readBack == valueToSave) {
                            Serial.printf("  VERIFIED: '%s'\n", readBack.c_str());
                        } else {
                            Serial.printf("  ERROR: Expected '%s', got '%s'\n", valueToSave.c_str(), readBack.c_str());
                        }
#else
                        (void)written;  // Suppress unused variable warning
#endif
                        break;
                    }

                    case EEPROM_BOOL:
                        prefs.putBool(item.key, item.value.boolVal);
#if DEBUG_EEPROM == 1
                        Serial.printf("> %s saved: %s\n", item.key, item.value.boolVal ? "true" : "false");
#endif
                        break;

                    case EEPROM_SAVE_DMS:
                        saveDmsToNvs();
                        break;
                }
                xSemaphoreGive(eepromMutex);
            }
        }
    }
}