#include "eeprom_task.h"
#include "config.h"
#include "globals.h"
#include "types.h"

void eepromTask(void *parameter) {
    eepromWriteItem_t item;
    
    while (true) {
        if (xQueueReceive(eepromWriteQueue, &item, portMAX_DELAY)) {
            if (xSemaphoreTake(eepromMutex, portMAX_DELAY)) {
                switch (item.type) {
                    case EEPROM_FLOAT:
                        prefs.putFloat(item.key, item.value.floatVal);
                        Serial.print("> ");
                        Serial.print(item.key);
                        Serial.print(" saved to eeprom: ");
                        Serial.println(item.value.floatVal);
                        break;
                        
                    case EEPROM_INT:
                        prefs.putInt(item.key, item.value.intVal);
                        Serial.print("> ");
                        Serial.print(item.key);
                        Serial.print(" saved to eeprom: ");
                        Serial.println(item.value.intVal);
                        break;
                        
                    case EEPROM_STRING: {
                        String valueToSave = String(item.value.stringVal);
                        size_t written = prefs.putString(item.key, valueToSave);

                        Serial.print("> ");
                        Serial.print(item.key);
                        Serial.print(" write attempt: ");
                        Serial.print(item.value.stringVal);
                        Serial.printf(" (returned %d bytes)\n", written);

                        // Immediately verify the write by reading it back
                        String readBack = prefs.getString(item.key, "VERIFY_FAILED");
                        if (readBack == valueToSave) {
                            Serial.printf("  VERIFIED: Read back '%s' successfully\n", readBack.c_str());
                        } else {
                            Serial.printf("  ERROR: Verification failed! Expected '%s', got '%s'\n",
                                        valueToSave.c_str(), readBack.c_str());
                        }
                        break;
                    }

                    case EEPROM_BOOL:
                        prefs.putBool(item.key, item.value.boolVal);
                        Serial.print("> ");
                        Serial.print(item.key);
                        Serial.print(" saved to eeprom: ");
                        Serial.println(item.value.boolVal ? "true" : "false");
                        break;
                }
                xSemaphoreGive(eepromMutex);
            }
        }
    }
}