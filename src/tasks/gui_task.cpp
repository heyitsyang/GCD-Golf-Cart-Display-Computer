#include "gui_task.h"
#include "globals.h"
#include "config.h"
#include <lvgl.h>
#include "ui_eez/ui.h"
#include "ui_eez/screens.h"
#include "ui_eez/styles.h"
#include "ui_eez/eez-flow.h"
#include "ui/venue_event_display.h"
#include "ui/chat_screen.h"
#include "ui/canned_screen.h"
#include "ui/settings2_screen.h"
#include "ui/settings_screen.h"
#include "ui/menu_screen.h"
#include "get_set_vars.h"
#include "hardware/display.h"

// Set by LV_EVENT_SCREEN_LOADED on objects.home — fires after FADE_IN animation
// completes, guaranteeing home screen pixels are fully rendered before the beep.
static bool s_homeScreenLoaded = false;
static void homeScreenLoadedCb(lv_event_t *) {
    s_homeScreenLoaded = true;
}


// Colour states for the GCI MAC label. Mirrors the mailbox row's health model
// one line below it: a neutral state that is not a fault, plus the two that are.
#define GCI_MAC_UNPAIRED  0   // white  — no GCI, a supported configuration
#define GCI_MAC_CONNECTED 1   // green  — paired and responding
#define GCI_MAC_LOST      2   // red    — paired but silent
#define GCI_MAC_UNKNOWN   0xFF

void updateEspnowGciMacColor() {
    // 0xFF forces a write on the first evaluation of each Settings 2 visit.
    static uint8_t last_state = GCI_MAC_UNKNOWN;

    // Only update if we're on the settings2 screen that has the GCI MAC label
    lv_obj_t* current_screen = lv_scr_act();
    if (current_screen == nullptr || current_screen != objects.settings2) {
        // Force a repaint on the next visit rather than trusting a cached
        // colour set before the user navigated away.
        last_state = GCI_MAC_UNKNOWN;
        return;
    }

    // Check if lbl_gci_mac_addr_value exists and is valid
    if (objects.lbl_gci_mac_addr_value == nullptr) {
        return;
    }

    // "NONE" is not an error: the GCI is optional, and running without one is a
    // deliberate choice that buys battery life. Red is reserved for a GCI that
    // was paired and has stopped answering, which is the only actionable case.
    // The length test matches the validity check espnowTask and sleep_manager
    // already use, so a malformed MAC reads as unpaired rather than as a fault.
    bool unpaired = (espnow_gci_mac_addr == "NONE" || espnow_gci_mac_addr.length() != 17);

    uint8_t state = unpaired                  ? GCI_MAC_UNPAIRED
                  : get_var_espnow_connected() ? GCI_MAC_CONNECTED
                                               : GCI_MAC_LOST;

    // Tracking the tri-state, not just the connected flag: unpairing an already
    // disconnected GCI leaves espnow_connected false both before and after, so a
    // bool cache would never notice and the label would stay red.
    if (state != last_state) {
        last_state = state;
        lv_color_t c = (state == GCI_MAC_CONNECTED) ? lv_color_hex(0xff00ff2d)
                     : (state == GCI_MAC_LOST)      ? lv_color_hex(0xffff0000)
                                                    : lv_color_white();
        lv_obj_set_style_text_color(objects.lbl_gci_mac_addr_value, c,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void checkGpsTimeStale() {
    static bool timeWasStale = false;
#if DEBUG_GPS
    static uint32_t s_lastGapLog = 0;
    static uint32_t s_staleStartMs = 0;
#endif
    const unsigned long GPS_TIME_TIMEOUT = MAX_GPS_TIME_STALENESS_SECS * 1000UL;

    uint32_t now32 = millis();
    bool timeIsStale = (lastGpsTimeUpdate == 0) || ((now32 - lastGpsTimeUpdate) > GPS_TIME_TIMEOUT);

#if DEBUG_GPS
    if (now32 - s_lastGapLog >= 30000) {
        s_lastGapLog = now32;
        Serial.printf("[GPS] gap=%lums / timeout=%lums\n",
                      lastGpsTimeUpdate ? (now32 - lastGpsTimeUpdate) : 99999UL,
                      (unsigned long)GPS_TIME_TIMEOUT);
    }
#endif

    if (timeIsStale != timeWasStale) {
#if DEBUG_GPS
        if (timeIsStale) {
            s_staleStartMs = now32;
            Serial.printf("[GPS_STALE] NO GPS written: gap=%lums lastUpdate=%lu\n",
                          lastGpsTimeUpdate ? (now32 - lastGpsTimeUpdate) : 99999UL,
                          lastGpsTimeUpdate);
        } else {
            Serial.printf("[GPS_STALE] Recovered: was stale for %lums\n",
                          now32 - s_staleStartMs);
        }
#endif
        if (xSemaphoreTake(gpsMutex, portMAX_DELAY)) {
            if (timeIsStale) {
                cur_date = String("NO GPS");
                hhmm_str = String("");
                hhmmss_str = String("");
                am_pm_str = String("");
            }
            xSemaphoreGive(gpsMutex);
        }
        timeWasStale = timeIsStale;
    }
}

void guiTask(void *parameter) {
    static uint32_t last_flag_set_time = 0;
    static uint32_t last_inactivity_check = 0;
    static uint32_t last_gps_time_check = 0;
    static lv_obj_t* previous_screen = nullptr;

    static bool first_render_signaled = false;
    static bool s_splashDoneSignaled = false;

    // Free the heap reservation held since setup(). Doing it here — right before
    // the first lv_timer_handler() — ensures nothing else allocates into the freed
    // 26 KB gap between the free and LVGL's 24 KB glyph draw-buffer request.
#if DEBUG_HEAP
    Serial.printf("[HEAP] guiTask pre-free: glyph_guard=%p free=%u max=%u\n",
                  glyph_guard, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
#endif
    if (glyph_guard) { free(glyph_guard); glyph_guard = nullptr; }
#if DEBUG_HEAP
    Serial.printf("[HEAP] guiTask post-free: free=%u max=%u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
#endif

    // LV_EVENT_SCREEN_LOADED fires after the FADE_IN animation finishes —
    // home pixels are guaranteed visible when s_homeScreenLoaded becomes true.
    lv_obj_add_event_cb(objects.home, homeScreenLoadedCb, LV_EVENT_SCREEN_LOADED, nullptr);

    while (true) {
        uint32_t now = millis();

        lv_tick_inc(now - lastTick);
        lastTick = now;
        lv_timer_handler();

        // Unblock meshtastic_task after the first render (~0.2s). The GCM position
        // config packet arrives early in the config dump; a short delay preserves it
        // in the 256-byte UART RX buffer so gpsConfigAttempted can be set → AWAKE sent.
        if (!first_render_signaled) {
            first_render_signaled = true;
            if (firstRenderDone) xSemaphoreGive(firstRenderDone);
        }
        // Unblock espnow_task only after the home screen loads (~3-4s). WiFi init
        // fragments the heap below the 24KB glyph draw-buffer threshold; delaying
        // it until the 172px splash glyph is no longer rendered prevents OOM.
        if (!s_splashDoneSignaled && s_homeScreenLoaded) {
            s_splashDoneSignaled = true;
            if (splashDone) xSemaphoreGive(splashDone);
        }
        ui_tick();

        // Fire one double-beep after restored DMs are visible on the home screen.
        // s_homeScreenLoaded is set by homeScreenLoadedCb via LV_EVENT_SCREEN_LOADED,
        // which LVGL fires at the end of the FADE_IN animation — guaranteed post-render.
        if (pendingDmRestoreBeep && s_homeScreenLoaded) {
            pendingDmRestoreBeep = false;
            tone_message();
        }

        // Drain chat / messaging UI updates queued from other tasks.
        chatScreenPump();
        cannedScreenPump();
        settings2ScreenPump();
        settingsScreenPump();
        menuScreenPump();

        // Update espnow GCI MAC address color on Settings2 screen
        updateEspnowGciMacColor();

        // Check GPS time staleness (every 1 second)
        if ((now - last_gps_time_check) >= 1000) {
            checkGpsTimeStale();
            last_gps_time_check = now;
        }

        // Check for screen changes and reset countdown if screen changed
        lv_obj_t* current_screen = lv_scr_act();
        if (current_screen != previous_screen) {
            // Check if we're leaving the Now Playing screen
            if (previous_screen == objects.now_playing && current_screen != objects.now_playing) {
                onNowPlayingScreenExit();
            }
            if (previous_screen == objects.meshtastic_messages && current_screen != objects.meshtastic_messages) {
                chatScreenFreeRows();
            }
            if (previous_screen == objects.meshtastic_canned_messages && current_screen != objects.meshtastic_canned_messages) {
                cannedScreenFreeRecipientBtn();
            }
            if (previous_screen == objects.settings2 && current_screen != objects.settings2) {
                settings2ScreenOnExit();
            }

#if DEBUG_HEAP
            {
                const char * sname = "?";
                if      (current_screen == objects.splash)                     sname = "SPLASH";
                else if (current_screen == objects.home)                       sname = "HOME";
                else if (current_screen == objects.menu)                       sname = "MENU";
                else if (current_screen == objects.meshtastic_messages)        sname = "MESSAGES";
                else if (current_screen == objects.meshtastic_canned_messages) sname = "CANNED";
                else if (current_screen == objects.settings)                   sname = "SETUP";
                else if (current_screen == objects.settings2)                  sname = "SETUP2";
                else if (current_screen == objects.vehicle)                    sname = "VEHICLE";
                else if (current_screen == objects.weather)                    sname = "WEATHER";
                else if (current_screen == objects.now_playing)                sname = "NOW_PLAYING";
                else if (current_screen == objects.num_entry)                  sname = "NUM_ENTRY";

                // ui_tick() refreshes tick_screen(g_currentScreen), not whatever
                // LVGL happens to be showing. Any screen change that bypasses
                // eez_flow_set/push/pop_screen() desyncs the two and silently
                // freezes every bound widget on the visible screen, so surface it
                // here rather than waiting for someone to notice a stopped clock.
                int16_t eezId = eez_flow_get_current_screen();
                lv_obj_t *eezScreen = (eezId > 0) ? ((lv_obj_t **)&objects)[eezId - 1] : nullptr;
                Serial.printf("[%s] enter: free=%u max=%u eez=%d%s\n",
                              sname,
                              (unsigned)ESP.getFreeHeap(),
                              (unsigned)ESP.getMaxAllocHeap(),
                              (int)eezId,
                              (eezScreen != current_screen) ? "  *** EEZ/LVGL DESYNC ***" : "");
            }
#endif
            previous_screen = current_screen;
            // Reset countdown when entering a new screen (except splash)
            if (current_screen != objects.splash) {
                set_var_screen_inactivity_countdown(SCREEN_INACTIVITY_TIMEOUT_MS);
            }
        }

        // Check if Now Playing screen needs updating when new data flag is set
        if (new_rx_data_flag) {
            // Record when flag was set (if this is the first time we see it)
            if (last_flag_set_time == 0) {
                last_flag_set_time = now;
            }

            // Check for Now Playing screen updates
            checkAndUpdateNowPlayingScreen();

            // Auto-reset flag after configured time
            if ((now - last_flag_set_time) >= NEW_RX_DATA_FLAG_RESET_TIME) {
                new_rx_data_flag = false;
                last_flag_set_time = 0;
            }
        } else {
            // Reset timer when flag is not set
            last_flag_set_time = 0;
        }

        // Handle inactivity countdown (check every 100ms)
        if ((now - last_inactivity_check) >= 100) {
            handleInactivityCountdown(now);
            last_inactivity_check = now;
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void handleInactivityCountdown(uint32_t now) {
    lv_obj_t* current_screen = lv_scr_act();

    // Skip countdown on splash screen
    if (current_screen == objects.splash) {
        set_var_screen_inactivity_countdown(0);
        return;
    }

    // Decrement countdown if active
    if (get_var_screen_inactivity_countdown() > 0) {
        int32_t remaining = get_var_screen_inactivity_countdown() - 100;
        if (remaining <= 0) {
            // Navigate to home with no animation — FADE_IN (triggered when EEZ sees
            // countdown=0) temporarily consumes ~45 KB, dropping largest_block below
            // glyph allocation threshold (2932B < 2944-3024B needed by Cart-60/REM-80).
            //
            // Must go through EEZ rather than lv_scr_load_anim(): replacePageHook()
            // is the only writer of g_currentScreen, and ui_tick() refreshes exactly
            // that one screen. A raw LVGL load leaves Home on the display while the
            // flow keeps ticking the screen we left, so the clock — and every other
            // expression-bound widget on Home — freezes until the user navigates
            // with a real UI control. LV_SCR_LOAD_ANIM_NONE is preserved; only the
            // bookkeeping changes.
            if (lv_scr_act() != objects.home) {
                eez_flow_set_screen(SCREEN_ID_HOME, LV_SCR_LOAD_ANIM_NONE, 0, 0);
            }
            set_var_screen_inactivity_countdown(SCREEN_INACTIVITY_TIMEOUT_MS);
            return;
        }
        set_var_screen_inactivity_countdown(remaining);
    }
}