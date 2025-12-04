#ifndef MESHTASTIC_TASK_H
#define MESHTASTIC_TASK_H

void meshtasticTask(void *parameter);

// Send wake notification message after boot
// Broadcasts message when Meshtastic connection is established
// Should be called from system task polling loop (runs once after connection)
void sendWakeNotificationOnBoot();

#endif // MESHTASTIC_TASK_H