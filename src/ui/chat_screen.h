#ifndef CHAT_SCREEN_H
#define CHAT_SCREEN_H

#include <Arduino.h>

// One-time wiring. Call from setup() AFTER ui_init().
void chatScreenInit();

// Rebuild the chat row list from the ring buffer using the current
// mesh_filter selection. Call from the GUI task only.
void chatScreenRefresh();

// Mark the chat list dirty. Safe to call from any task; the GUI task
// drains the flag in chatScreenPump() and rebuilds rows.
void chatScreenRequestRefresh();

// GUI-task drain hook. Calls chatScreenRefresh() if a refresh is
// pending OR if filter changed since last build.
void chatScreenPump();

// Hide the row pool on Messages screen exit. Rows stay allocated (pre-alloc
// fix); hidden rows are shown again on next chatScreenRefresh() call.
void chatScreenFreeRows();

// Pre-allocate the row pool from clean boot heap. Call from setup() after
// chatScreenInit(). Eliminates late fragmented-heap allocation that causes OOM.
void chatScreenPreAllocRows();

#endif // CHAT_SCREEN_H
