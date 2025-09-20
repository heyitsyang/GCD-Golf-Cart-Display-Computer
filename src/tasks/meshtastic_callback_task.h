#ifndef CALLBACK_TASK_H
#define CALLBACK_TASK_H

#include <Arduino.h>

void meshtasticCallbackTask(void *parameter);
//void connected_callback(mt_node_t *node, mt_nr_progress_t progress);
void text_message_callback(uint32_t from, uint32_t to, uint8_t channel, const char *text);

#endif // CALLBACK_TASK_H