#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <Arduino.h>

String prefix_zero(int max2digits);
int make12hr(int time_hr);
String am_pm(int time_hr);
const char* getMonthAbbr(int monthNumber);
const char* getDayAbbr(int weekday_num);

#endif // TIME_UTILS_H