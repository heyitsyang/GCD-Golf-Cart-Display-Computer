#ifndef GUI_TASK_H
#define GUI_TASK_H

#include <stdint.h>

void guiTask(void *parameter);
void handleInactivityCountdown(uint32_t now);
void updateEspnowIndicatorColor();

#endif // GUI_TASK_H