#include "time_utils.h"

String prefix_zero(int max2digits) {
    return (max2digits < 10) ? "0" + String(max2digits) : String(max2digits);
}

int make12hr(int time_hr) {
    if (time_hr == 0) return 12;
    return (time_hr > 12) ? time_hr - 12 : time_hr;
}

String am_pm(int time_hr) {
    return (time_hr >= 12) ? String("PM") : String("AM");
}

const char* getMonthAbbr(int monthNumber) {
    static const char* monthAbbreviations[] = {
        "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    
    if (monthNumber >= 1 && monthNumber <= 12) {
        return monthAbbreviations[monthNumber];
    }
    return "Inv";
}

const char* getDayAbbr(int weekday_num) {
    const char* day_abbreviations[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    
    if (weekday_num >= 1 && weekday_num <= 7) {
        return day_abbreviations[weekday_num - 1];
    }
    return "Err";
}