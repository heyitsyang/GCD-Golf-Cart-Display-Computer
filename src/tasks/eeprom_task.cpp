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
                        Serial.print("< ");
                        Serial.print(item.key);
                        Serial.print(" saved to eeprom: ");
                        Serial.println(item.value.intVal);
                        break;
                        
                    case EEPROM_STRING:
                        prefs.putString(item.key, String(item.value.stringVal));
                        Serial.print("> ");
                        Serial.print(item.key);
                        Serial.print(" saved to eeprom: ");
                        Serial.println(item.value.stringVal);
                        break;

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