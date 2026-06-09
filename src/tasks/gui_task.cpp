#include "gui_task.h"
#include "globals.h"
#include "config.h"
#include <lvgl.h>
#include "ui_eez/ui.h"
#include "ui_eez/screens.h"
#include "ui_eez/styles.h"
#include "ui/venue_event_display.h"
#include "ui/chat_screen.h"
#include "ui/canned_screen.h"
#include "ui/settings2_screen.h"
#include "get_set_vars.h"
#include "hardware/display.h"

// Set by LV_EVENT_SCREEN_LOADED on objects.home — fires after FADE_IN animation
// completes, guaranteeing home screen pixels are fully rendered before the beep.
static bool s_homeScreenLoaded = false;
static void homeScreenLoadedCb(lv_event_t *) {
    s_homeScreenLoaded = true;
}


void updateEspnowGciMacColor() {
    static bool last_espnow_connected_state = false;
    static bool initialized = false;

    // Only update if we're on the settings2 screen that has obj5 (GCI MAC address label)
    lv_obj_t* current_screen = lv_scr_act();
    if (current_screen == nullptr || current_screen != objects.settings2) {
        // Reset initialization when not on settings2 screen
        initialized = false;
        return;
    }

    // Check if lbl_gci_mac_addr_value exists and is valid
    if (objects.lbl_gci_mac_addr_value == nullptr) {
        return;
    }

    bool current_state = get_var_espnow_connected();

    // Initialize or update when state changes
    if (!initialized || current_state != last_espnow_connected_state) {
        if (current_state) {
            // Connected - apply green color (#00ff2d)
            lv_obj_set_style_text_color(objects.lbl_gci_mac_addr_value, lv_color_hex(0xff00ff2d), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            // Disconnected - apply red color (#ff0000)
            lv_obj_set_style_text_color(objects.lbl_gci_mac_addr_value, lv_color_hex(0xffff0000), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        last_espnow_connected_state = current_state;
        initialized = true;
    }
}

void checkGpsTimeStale() {
    static bool timeWasStale = false;
    const unsigned long GPS_TIME_TIMEOUT = MAX_GPS_TIME_STALENESS_SECS * 1000UL;  // Convert seconds to milliseconds

    // Check if GPS time is stale (no update for more than MAX_GPS_TIME_STALENESS_SECS seconds)
    // lastGpsTimeUpdate is 0 at boot until first GPS time is received
    bool timeIsStale = (lastGpsTimeUpdate == 0) || ((millis() - lastGpsTimeUpdate) > GPS_TIME_TIMEOUT);

    // Only update display if staleness state changed
    if (timeIsStale != timeWasStale) {
        if (xSemaphoreTake(gpsMutex, portMAX_DELAY)) {
            if (timeIsStale) {
                // GPS time is stale - show blank clock and "NO GPS"
                cur_date = String("NO GPS");
                hhmm_str = String("");
                hhmmss_str = String("");
                am_pm_str = String("");
            }
            // Note: When GPS time becomes valid again, gps_task will update these strings
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
            if (lv_scr_act() != objects.home) {
                lv_scr_load_anim(objects.home, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
            }
            set_var_screen_inactivity_countdown(SCREEN_INACTIVITY_TIMEOUT_MS);
            return;
        }
        set_var_screen_inactivity_countdown(remaining);
    }
}