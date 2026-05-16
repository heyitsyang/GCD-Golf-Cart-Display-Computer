#include "favorites.h"
#include "storage/preferences_manager.h"
#include "globals.h"

static uint32_t s_favorites[MAX_FAVORITES];
static char     s_favNames[MAX_FAVORITES][5];
static uint8_t  s_favCount = 0;

void favoritesLoad() {
    s_favCount = (uint8_t)prefs.getInt("fav_count", 0);
    if (s_favCount > MAX_FAVORITES) s_favCount = MAX_FAVORITES;
    for (uint8_t i = 0; i < s_favCount; i++) {
        char key[10];
        snprintf(key, sizeof(key), "fav_%u", (unsigned)i);
        s_favorites[i] = (uint32_t)prefs.getInt(key, 0);

        char nmkey[10];
        snprintf(nmkey, sizeof(nmkey), "favnm_%u", (unsigned)i);
        String nm = prefs.getString(nmkey, "");
        strncpy(s_favNames[i], nm.c_str(), 4);
        s_favNames[i][4] = '\0';
    }
}

bool favoritesAdd(uint32_t nodeId, const char* shortName) {
    if (favoritesContains(nodeId)) return true;
    if (s_favCount >= MAX_FAVORITES) return false;

    s_favorites[s_favCount] = nodeId;
    if (shortName && shortName[0]) {
        strncpy(s_favNames[s_favCount], shortName, 4);
        s_favNames[s_favCount][4] = '\0';
    } else {
        s_favNames[s_favCount][0] = '\0';
    }

    char key[10];
    snprintf(key, sizeof(key), "fav_%u", (unsigned)s_favCount);
    queuePreferenceWrite(key, (int)nodeId);

    char nmkey[10];
    snprintf(nmkey, sizeof(nmkey), "favnm_%u", (unsigned)s_favCount);
    queuePreferenceWrite(nmkey, String(s_favNames[s_favCount]));

    s_favCount++;
    queuePreferenceWrite("fav_count", (int)s_favCount);
    return true;
}

void favoritesRemove(uint32_t nodeId) {
    for (uint8_t i = 0; i < s_favCount; i++) {
        if (s_favorites[i] != nodeId) continue;
        for (uint8_t j = i; j < s_favCount - 1; j++) {
            s_favorites[j] = s_favorites[j + 1];
            memcpy(s_favNames[j], s_favNames[j + 1], 5);

            char key[10];
            snprintf(key, sizeof(key), "fav_%u", (unsigned)j);
            queuePreferenceWrite(key, (int)s_favorites[j]);

            char nmkey[10];
            snprintf(nmkey, sizeof(nmkey), "favnm_%u", (unsigned)j);
            queuePreferenceWrite(nmkey, String(s_favNames[j]));
        }
        s_favCount--;
        // Clear the vacated last slot in NVS
        char key[10];
        snprintf(key, sizeof(key), "fav_%u", (unsigned)s_favCount);
        queuePreferenceWrite(key, (int)0);

        char nmkey[10];
        snprintf(nmkey, sizeof(nmkey), "favnm_%u", (unsigned)s_favCount);
        queuePreferenceWrite(nmkey, String(""));

        queuePreferenceWrite("fav_count", (int)s_favCount);
        return;
    }
}

void favoritesUpdateName(uint32_t nodeId, const char* shortName) {
    if (!shortName || !shortName[0]) return;
    for (uint8_t i = 0; i < s_favCount; i++) {
        if (s_favorites[i] != nodeId) continue;
        if (s_favNames[i][0] != '\0') return;  // already has a name
        strncpy(s_favNames[i], shortName, 4);
        s_favNames[i][4] = '\0';
        char nmkey[10];
        snprintf(nmkey, sizeof(nmkey), "favnm_%u", (unsigned)i);
        queuePreferenceWrite(nmkey, String(s_favNames[i]));
        return;
    }
}

bool favoritesContains(uint32_t nodeId) {
    for (uint8_t i = 0; i < s_favCount; i++) {
        if (s_favorites[i] == nodeId) return true;
    }
    return false;
}

uint8_t  favoritesCount() { return s_favCount; }

uint32_t favoritesGet(uint8_t index) {
    if (index >= s_favCount) return 0;
    return s_favorites[index];
}

const char* favoritesGetName(uint8_t index) {
    if (index >= s_favCount) return "";
    return s_favNames[index];
}
