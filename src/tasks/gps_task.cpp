#include "gps_task.h"
#include "config.h"
#include "globals.h"
#include "utils/time_utils.h"
#include "hardware/display.h"
#include <TimeLib.h>

void gpsTask(void *parameter) {
    time_t sunrise_t, sunset_t;
    
    while (true) {
        unsigned long currentGPSms = millis();
        int elapsed_gps_read = currentGPSms - previousGPSms;
        
        if (elapsed_gps_read >= GPS_READ_INTERVAL) {
            if (xSemaphoreTake(gpsMutex, portMAX_DELAY)) {
                previousGPSms = currentGPSms;
                
                // Process all available GPS data
                gps_fix latestValidFix;
                bool foundValidLocation = false;
                bool foundValidTime = false;
                
                while (gpsSerial.available() > 0) {
                    char c = gpsSerial.read();
                    if (gps.decode(c) == NMEAGPS::DECODE_COMPLETED) {
                        fix = gps.fix();
                        
                        if (fix.valid.date && fix.valid.time) {
                            latestValidFix = fix;
                            foundValidTime = true;
                        }
                        
                        if (fix.valid.location) {
                            latestValidFix = fix;
                            foundValidLocation = true;
                        }
                    }
                }
                
                // Update time from GPS
                if (foundValidTime) {
                    setTime(latestValidFix.dateTime.hours, latestValidFix.dateTime.minutes, 
                           latestValidFix.dateTime.seconds, latestValidFix.dateTime.date, 
                           latestValidFix.dateTime.month, latestValidFix.dateTime.full_year());
                    
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
                    cur_date = String(getDayAbbr(localDayOfWeek)) + ", " + 
                              String(getMonthAbbr(localMonth)) + " " + String(localDay);
                    hhmm_str = String(make12hr(localHour)) + ":" + String(prefix_zero(localMinute));
                    hhmmss_str = String(make12hr(localHour)) + ":" + 
                                String(prefix_zero(localMinute)) + String(prefix_zero(localSecond));
                    am_pm_str = am_pm(localHour);
                }
                
                // Update location data
                if (foundValidLocation) {
                    latitude = String(latestValidFix.latitude(), 6);
                    longitude = String(latestValidFix.longitude(), 6);
                    
                    if (latestValidFix.valid.altitude) {
                        altitude = String(latestValidFix.altitude(), 2);
                    }
                    
                    // Handle HDOP
                    #ifdef NMEAGPS_PARSE_HDOP
                    if (latestValidFix.valid.hdop) {
                        hdop = latestValidFix.hdop / 1000.0;
                    } else if (latestValidFix.valid.satellites && latestValidFix.satellites > 0) {
                        hdop = (latestValidFix.satellites >= 6) ? 1.5 : 
                              (latestValidFix.satellites >= 4) ? 2.0 : 5.0;
                    }
                    #else
                    if (latestValidFix.valid.satellites && latestValidFix.satellites > 0) {
                        hdop = (latestValidFix.satellites >= 6) ? 1.5 : 
                              (latestValidFix.satellites >= 4) ? 2.0 : 5.0;
                    }
                    #endif
                    
                    // Update satellites/HDOP display
                    if (latestValidFix.valid.satellites) {
                        sats_hdop = String(latestValidFix.satellites) + "/" + String(hdop, 2);
                    }
                    
                    // Update speed and heading if reliable
                    if (hdop < max_hdop) {
                        if (latestValidFix.valid.speed) {
                            float speedMph = latestValidFix.speed_mph();
                            avg_speed_calc = avgSpeed.reading(speedMph);  // Float version
                            avg_speed = (int32_t)avg_speed_calc;  // Integer version for display
                        }
                        
                        if (latestValidFix.valid.heading) {
                            float headingDeg = latestValidFix.heading_cd() / 100.0;
                            float avgHeading = avgAzimuthDeg.reading(headingDeg);
                            
                            const char* directions[] = {"N", "NNE", "NE", "ENE", "E", "ESE", 
                                                       "SE", "SSE", "S", "SSW", "SW", "WSW", 
                                                       "W", "WNW", "NW", "NNW"};
                            int dirIndex = (int)((avgHeading + 11.25) / 22.5) % 16;
                            heading = String(directions[dirIndex]);
                        }
                    }
                    
                    // Update backlight based on sunrise/sunset
                    if (localDay != old_localDay) {
                        sun.calculate(localTime, tcr->offset, sunrise_t, sunset_t);
                    }
                    
                    if ((localTime > sunrise_t) && (localTime < sunset_t)) {
                        setBacklight((day_backlight * 20) + 55);
                    } else {
                        setBacklight(night_backlight * 20);
                    }
                    
                    // Calculate distance traveled
                    float incDistance = avg_speed * elapsed_gps_read / 3600000;
                    accumDistance = accumDistance + incDistance;
                    
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
#endif
                }
                
                xSemaphoreGive(gpsMutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}