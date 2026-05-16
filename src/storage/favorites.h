#ifndef FAVORITES_H
#define FAVORITES_H

#include <Arduino.h>

#define MAX_FAVORITES 8

// Load favorites from NVS. Call once from setup() after initPreferences().
void     favoritesLoad();

// Add a node to favorites. Returns false if list is already full.
// No-op (returns true) if nodeId is already present.
// shortName: up to 4-char Meshtastic short name (pass nullptr or "" if unknown).
bool     favoritesAdd(uint32_t nodeId, const char* shortName = nullptr);

// Remove a node from favorites. No-op if not present.
void     favoritesRemove(uint32_t nodeId);

// Update the stored short name for a node if it is a favorite and its name is
// currently empty. Safe to call from the GUI task; no-op if name already set.
void     favoritesUpdateName(uint32_t nodeId, const char* shortName);

bool        favoritesContains(uint32_t nodeId);
uint8_t     favoritesCount();
uint32_t    favoritesGet(uint8_t index);      // index 0 = oldest
const char* favoritesGetName(uint8_t index);  // returns "" if no name stored

#endif // FAVORITES_H
