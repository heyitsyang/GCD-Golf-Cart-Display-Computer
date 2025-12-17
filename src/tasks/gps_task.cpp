#include "gps_task.h"
#include "config.h"
#include "globals.h"
#include "utils/time_utils.h"
#include "hardware/display.h"
#include "storage/preferences_manager.h"
#include <TimeLib.h>

// Compass direction lookup table
static const char* COMPASS_DIRECTIONS[] = {
    "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
    "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
};

/**
 * Convert heading in degrees to compass direction string
 */
static const char* headingToCompass(float degrees) {
    int index = (int)((degrees + 11.25) / 22.5) % 16;
    return COMPASS_DIRECTIONS[index];
}

// Previous speed for stop detection
static float previousSpeed = 0.0;

// Previous location for position-based distance calculation
static NeoGPS::Location_t lastLocation;
static bool hasLastLocation = false;

// Distance tracking for periodic EEPROM saves
static float lastSavedAccumDistance = 0.0;
static float lastSavedTripDistance = 0.0;
static const float SAVE_INTERVAL_MILES = 0.5;  // Save every 0.5 miles

// Hour tracking for service reminder (hybrid GPS/millis() with fallback)
static time_t lastMovementGpsTime = 0;      // GPS time_t of last movement (UTC seconds since epoch)
static unsigned long lastMovementMillis = 0; // millis() backup when GPS unavailable
static bool hoursTrackingInitialized = false;  // Flag to initialize from hrs_since_svc on first use
static int32_t lastSavedHrsSinceSvc = 0;
static const int32_t SAVE_INTERVAL_HOURS = 1;  // Save every 1 hour
static int32_t remainderSeconds = 0;  // Accumulator for sub-tenth hour precision

// Satellite count persistence - filter brief dropouts
static uint8_t lastValidSatCount = 0;
static uint8_t zeroSatConsecutiveCount = 0;
static const uint8_t ZERO_SAT_THRESHOLD = 3;  // Require 3 consecutive zero readings

// Track at_home state changes
static bool old_at_home = false;

/**
 * Update speed from GPS fix
 * When speed is valid, use it (with dither filtering).
 * When speed is invalid but location is valid, assume stationary (speed = 0).
 * Uses aggressive zero-out when speed is low and decreasing to improve stop response.
 */
static void updateSpeed(const gps_fix& fix) {
    if (fix.valid.speed) {
        float speedMph = fix.speed_mph();

        // Filter out GPS dither when stationary
        if (speedMph < MIN_SPEED_FILTER_MPH) {
            speedMph = 0.0;
        }
        // Aggressive stop detection: if speed is low (<4 mph) and decreasing, zero out
        else if (speedMph < 4.0 && speedMph < previousSpeed) {
            speedMph = 0.0;
        }

        previousSpeed = fix.speed_mph();  // Store raw GPS speed for comparison
        avg_speed_calc = speedMph;
        avg_speed = (int32_t)avg_speed_calc;
    }
    // If speed invalid but we were stopped or slowing down - can assume still stopped/stopping
    else if (fix.valid.location && avg_speed_calc < 5.0) {
        // Only zero speed if we were already slow - retain speed if cruising
        avg_speed_calc = 0.0;
        avg_speed = 0;
        previousSpeed = 0.0;
    }
    // Otherwise retain last known speed when invalid (e.g., cruising at 5+ mph)
}

/**
 * Update heading from GPS fix
 * Only updates when valid; retains last known good value otherwise
 */
static void updateHeading(const gps_fix& fix) {
    if (!fix.valid.heading) return;

    float degrees = fix.heading_cd() / 100.0;
    heading = String(headingToCompass(degrees));
}

/**
 * Update time and date display strings from GPS fix
 */
static void updateTimeDisplay(const gps_fix& fix) {
    if (!fix.valid.date || !fix.valid.time) return;

    // Set system time from GPS
    setTime(fix.dateTime.hours, fix.dateTime.minutes,
            fix.dateTime.seconds, fix.dateTime.date,
            fix.dateTime.month, fix.dateTime.full_year());

    // Convert to local time
    utcTime = now();
    localTime = myTZ.toLocal(utcTime, &tcr);
    localYear = year(localTime);
    localMonth = month(localTime);
    localDay = day(localTime);
    localHour = hour(localTime);
    localMinute = minute(localTime);
    localSecond = second(localTime);
    localDayOfWeek = weekday(localTime);

    // Update display strings
    cur_date = String(getDayAbbr(localDayOfWeek)) + ", " + String(getMonthAbbr(localMonth)) + " " + String(localDay);
    hhmm_str = String(make12hr(localHour)) + ":" + String(prefix_zero(localMinute));
    hhmmss_str = String(make12hr(localHour)) + ":" +
                 String(prefix_zero(localMinute)) + String(prefix_zero(localSecond));
    am_pm_str = am_pm(localHour);

    lastGpsTimeUpdate = millis();
}

/**
 * Update HDOP value from fix or estimate from satellite count
 */
static void updateHdop(const gps_fix& fix) {
    #ifdef NMEAGPS_PARSE_HDOP
    if (fix.valid.hdop) {
        hdop = fix.hdop / 1000.0;
        return;
    }
    #endif

    // Estimate HDOP from satellite count if not directly available
    if (fix.valid.satellites) {
        if (fix.satellites >= 6) hdop = 1.5;
        else if (fix.satellites >= 4) hdop = 2.0;
        else hdop = 99.0;  // No fix or poor fix when < 4 satellites
    }
}

/**
 * Update backlight based on sunrise/sunset times
 */
static void updateBacklight(time_t& sunrise_t, time_t& sunset_t) {
    // Recalculate sunrise/sunset when day changes
    if (localDay != old_localDay) {
        sun.calculate(localTime, tcr->offset, sunrise_t, sunset_t);
    }

    // Set backlight based on day/night
    if (localTime > sunrise_t && localTime < sunset_t) {
        setBacklight((day_backlight * 20) + 55);
    } else {
        setBacklight(night_backlight * 20);
    }
}

/**
 * Check and update home location status
 * Handles setting home location when triggered and calculates if we're currently at home
 */
static void updateHomeLocation(const gps_fix& fix) {
    if (!fix.valid.location) return;

    // Check if user triggered "set home location"
    if (set_home_loc) {
        float lat = fix.latitude();
        float lon = fix.longitude();

        // Only save if we have valid non-zero coordinates
        if (lat != 0.0 && lon != 0.0) {
            homeLatitude = lat;
            homeLongitude = lon;
            homeLocationSet = true;

            Serial.println("Setting home location:");
            Serial.print("  Latitude: "); Serial.println(homeLatitude, 6);
            Serial.print("  Longitude: "); Serial.println(homeLongitude, 6);

            // Save to EEPROM
            queuePreferenceWrite("home_lat", homeLatitude);
            queuePreferenceWrite("home_lon", homeLongitude);
        } else {
            Serial.println("Cannot set home location: GPS coordinates are invalid (0.0, 0.0)");
        }

        // Reset the trigger regardless
        set_home_loc = false;
    }

    // Calculate if we're at home
    if (homeLocationSet) {
        // Create a location object for home coordinates
        // Must cast to int32_t to prevent overflow
        int32_t homeLat_int32 = (int32_t)(homeLatitude * 1e7);
        int32_t homeLon_int32 = (int32_t)(homeLongitude * 1e7);
        NeoGPS::Location_t homeLocation(homeLat_int32, homeLon_int32);

        // Calculate distance in meters
        float distanceMeters = fix.location.DistanceKm(homeLocation) * 1000.0;

        // Update at_home status based on fence radius
        at_home = (distanceMeters <= home_gps_fence_radius_m);

        // Print debug message when at_home state changes
        if (at_home != old_at_home) {
            if (at_home) {
                Serial.print("*** ENTERED home geo-fence (distance: ");
                Serial.print(distanceMeters, 1);
                Serial.println(" meters) ***");
            } else {
                Serial.print("*** LEFT home geo-fence (distance: ");
                Serial.print(distanceMeters, 1);
                Serial.println(" meters) ***");
            }
            old_at_home = at_home;
        }
    } else {
        // No home location set yet
        at_home = false;
        old_at_home = false;
    }
}

/**
 * Update location-related data from GPS fix
 */
static void updateLocation(const gps_fix& fix, time_t& sunrise_t, time_t& sunset_t) {
    if (!fix.valid.location) return;

    // Update both old and new coordinate variables
    latitude = String(fix.latitude(), 6);
    longitude = String(fix.longitude(), 6);
    cur_lat = latitude;
    cur_long = longitude;

    if (fix.valid.altitude) {
        altitude = String(fix.altitude(), 2);
    }

    updateHdop(fix);

    // Update satellite count with persistence to filter brief dropouts
    if (fix.valid.satellites) {
        if (fix.satellites > 0) {
            // Valid non-zero count - update display and reset dropout counter
            lastValidSatCount = fix.satellites;
            zeroSatConsecutiveCount = 0;
            sats_hdop = String(fix.satellites) + "/" + String(hdop, 2);
        } else {
            // Zero satellites reported - only update display after consecutive zeros
            zeroSatConsecutiveCount++;
            if (zeroSatConsecutiveCount >= ZERO_SAT_THRESHOLD) {
                lastValidSatCount = 0;
                sats_hdop = "0/" + String(hdop, 2);
            }
            // Otherwise keep showing lastValidSatCount (display unchanged)
        }
    }

    updateBacklight(sunrise_t, sunset_t);

    // Accumulate distance traveled using hybrid position-based calculation
    if (hasLastLocation) {
        float posDistance = fix.location.DistanceMiles(lastLocation);
        float posSpeed = posDistance * 3600.0;  // Convert to mph (assumes ~1Hz)

        bool shouldAccumulate = false;

        // Use GPS Doppler speed as primary gate
        if (fix.valid.speed) {
            float dopplerSpeed = fix.speed_mph();

            // Only accumulate when GPS confirms we're moving
            if (dopplerSpeed > MIN_SPEED_FILTER_MPH) {
                // Position speed must be reasonable
                if (posSpeed < 30.0 && posDistance > 0.0005) {  // >2.6 feet minimum
                    shouldAccumulate = true;
                }
            }
            // If Doppler says stopped, don't accumulate (filters jitter)
        } else {
            // No Doppler speed available - use distance threshold only
            if (posDistance > 0.002 && posSpeed < 30.0) {  // >10 feet minimum
                shouldAccumulate = true;
            }
        }

        if (shouldAccumulate) {
            accum_distance += posDistance;
            trip_distance += posDistance;

            // Rollover at 100,000 miles (display limit is 99999.9)
            if (accum_distance >= 100000.0) {
                accum_distance = 0.0;
                lastSavedAccumDistance = 0.0;
            }
            if (trip_distance >= 100000.0) {
                trip_distance = 0.0;
                lastSavedTripDistance = 0.0;
            }

            /**
             * Accumulate driving hours (only when moving, same as distance)
             *
             * Hybrid time tracking system:
             * - PRIMARY: GPS time (atomic clock accurate, no drift)
             * - FALLBACK: millis() when GPS signal unavailable (slight drift acceptable)
             *
             * Storage format: hrs_since_svc is ALWAYS stored as tenths of hours
             * Conversion: 360 seconds = 0.1 hours (1 tenth)
             *
             * Example: 45 minutes = 2700 seconds = 7.5 tenths = stored as 7 in hrs_since_svc
             */

            // Initialize hours tracking on first movement
            if (!hoursTrackingInitialized) {
                lastSavedHrsSinceSvc = hrs_since_svc;
                hoursTrackingInitialized = true;
            }

            int32_t elapsedSeconds = 0;
            bool timeValid = false;

            // Attempt to use GPS time first (preferred for accuracy)
            if (fix.valid.date && fix.valid.time) {
                time_t currentGpsTime = now();  // GPS-synced UTC time from TimeLib

                if (lastMovementGpsTime > 0) {
                    elapsedSeconds = currentGpsTime - lastMovementGpsTime;

                    // Safety check: only accept reasonable time deltas (0-10 seconds)
                    // Filters GPS glitches, device reboots, or long signal loss
                    if (elapsedSeconds > 0 && elapsedSeconds <= 10) {
                        timeValid = true;
                    }
                }

                // Update GPS time reference for next iteration
                lastMovementGpsTime = currentGpsTime;
                lastMovementMillis = millis();  // Sync millis backup with GPS time
            }
            // Fallback to millis() if GPS time unavailable (tunnel, parking garage, etc.)
            else if (lastMovementMillis > 0) {
                unsigned long currentMillis = millis();
                unsigned long elapsedMillis = currentMillis - lastMovementMillis;

                // Convert millis to seconds
                elapsedSeconds = elapsedMillis / 1000;

                // Safety check: only accept reasonable time deltas (0-10 seconds)
                // Protects against millis() rollover (every 49.7 days)
                if (elapsedSeconds > 0 && elapsedSeconds <= 10) {
                    timeValid = true;
                }

                // Update millis reference for next iteration
                lastMovementMillis = currentMillis;
            }
            // First movement detected - initialize time references
            else {
                lastMovementMillis = millis();
                // GPS time will be set on next iteration when available
            }

            // Accumulate elapsed time if valid
            if (timeValid && elapsedSeconds > 0) {
                remainderSeconds += elapsedSeconds;

                // When we've accumulated 360 seconds (1 tenth hour), increment hrs_since_svc
                int32_t tenthsToAdd = remainderSeconds / 360;
                if (tenthsToAdd > 0) {
                    hrs_since_svc += tenthsToAdd;
                    remainderSeconds -= (tenthsToAdd * 360);
                }

                // Save to EEPROM every 1.0 hours of driving (10 tenths)
                if (hrs_since_svc - lastSavedHrsSinceSvc >= (SAVE_INTERVAL_HOURS * 10)) {
                    queuePreferenceWrite("hrs_since_svc", hrs_since_svc);
                    lastSavedHrsSinceSvc = hrs_since_svc;
                }
            }

            // Update display strings with 1 decimal place precision
            odometer = String(accum_distance, 1);
            trip_odometer = String(trip_distance, 1);

            // Save to EEPROM periodically (every 0.5 miles)
            if (accum_distance - lastSavedAccumDistance >= SAVE_INTERVAL_MILES) {
                queuePreferenceWrite("accumDistance", accum_distance);
                lastSavedAccumDistance = accum_distance;
            }

            if (trip_distance - lastSavedTripDistance >= SAVE_INTERVAL_MILES) {
                queuePreferenceWrite("tripDistance", trip_distance);
                lastSavedTripDistance = trip_distance;
            }
        }
    }

    // Update last location for next iteration
    lastLocation = fix.location;
    hasLastLocation = true;

#if DEBUG_GPS == 1
    Serial.print("\nLAT: ");
    Serial.println(latitude);
    Serial.print("LONG: ");
    Serial.println(longitude);
    Serial.print("AVG_SPEED (mph) = ");
    Serial.println(avg_speed);
    Serial.print("DIRECTION = ");
    Serial.println(heading);
    Serial.print("Sats/HDOP = ");
    Serial.println(sats_hdop);
    if (hasLastLocation) {
        Serial.print("Pos distance (mi) = ");
        Serial.println(fix.location.DistanceMiles(lastLocation), 6);
        Serial.print("Accum distance (mi) = ");
        Serial.println(accum_distance, 3);
    }
#endif
}

/**
 * GPS Task - processes NMEA data and updates global GPS variables
 *
 * Uses NeoGPS library with EXPLICIT_MERGING mode. The gps.available() function
 * returns true after LAST_SENTENCE_IN_INTERVAL (RMC) arrives, providing a
 * complete merged fix with data from all sentences in the update interval.
 */
void gpsTask(void *parameter) {
    time_t sunrise_t, sunset_t;

    Serial.println("GPS Task started");

    while (true) {
        if (xSemaphoreTake(gpsMutex, portMAX_DELAY)) {

            // Process all available merged fixes
            while (gps.available(gpsSerial)) {
                fix = gps.read();

                updateSpeed(fix);
                updateHeading(fix);
                updateTimeDisplay(fix);
                updateLocation(fix, sunrise_t, sunset_t);
                updateHomeLocation(fix);
            }

            xSemaphoreGive(gpsMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
