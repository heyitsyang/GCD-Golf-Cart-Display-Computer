#ifndef VENUE_EVENT_DISPLAY_H
#define VENUE_EVENT_DISPLAY_H

#include <lvgl.h>

void displayVenueEventTable(const char* dataString);

// Check if the Now Playing screen needs updating and refresh if needed
void checkAndUpdateNowPlayingScreen();

// Call when leaving the Now Playing screen
void onNowPlayingScreenExit();

// This is the external C function called from EEZ Studio actions
extern "C" void action_display_now_playing(lv_event_t *e);

#endif // VENUE_EVENT_DISPLAY_H