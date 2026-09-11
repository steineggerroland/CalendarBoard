#include "Board.h"
#include <string.h>
#include <limits.h>

namespace board {
namespace {
bool leap(int year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }
int monthDays(int year, int month) {
    const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return days[month - 1] + (month == 2 && leap(year) ? 1 : 0);
}
int digits(const char* text, size_t count) {
    int value = 0;
    for (size_t i = 0; i < count; ++i) {
        if (text[i] < '0' || text[i] > '9') return -1;
        value = value * 10 + text[i] - '0';
    }
    return value;
}
}
int dateOf(const tm& value) { return (value.tm_year + 1900) * 10000 + (value.tm_mon + 1) * 100 + value.tm_mday; }
bool parseDate(const char* text, int& date) {
    if (!text || strlen(text) != 10 || text[4] != '-' || text[7] != '-') return false;
    int year = digits(text, 4), month = digits(text + 5, 2), day = digits(text + 8, 2);
    // The pinned ESP8266 toolchain uses signed 32-bit time_t.
    if (year < 2000 || year > 2037 || month < 1 || month > 12 || day < 1 || day > monthDays(year, month)) return false;
    date = year * 10000 + month * 100 + day;
    return true;
}
bool parseTimestamp(const char* text, time_t& utc) {
    if (!text) return false;
    size_t size = strlen(text);
    if ((size != 20 && size != 25) || text[10] != 'T' || text[13] != ':' || text[16] != ':') return false;
    char dateText[11]; memcpy(dateText, text, 10); dateText[10] = 0;
    int date;
    if (!parseDate(dateText, date)) return false;
    int hour = digits(text + 11, 2), minute = digits(text + 14, 2), second = digits(text + 17, 2);
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) return false;
    int offset = 0;
    if (size == 20) { if (text[19] != 'Z') return false; }
    else {
        if ((text[19] != '+' && text[19] != '-') || text[22] != ':') return false;
        int oh = digits(text + 20, 2), om = digits(text + 23, 2);
        if (oh < 0 || oh > 14 || om < 0 || om > 59 || (oh == 14 && om != 0)) return false;
        offset = (oh * 60 + om) * 60 * (text[19] == '+' ? 1 : -1);
    }
    int year = date / 10000, month = date / 100 % 100;
    int64_t days = 0;
    for (int y = 1970; y < year; ++y) days += leap(y) ? 366 : 365;
    for (int m = 1; m < month; ++m) days += monthDays(year, m);
    days += date % 100 - 1;
    int64_t seconds = days * 86400 + hour * 3600 + minute * 60 + second - offset;
    if (seconds < 0 || seconds > INT32_MAX) return false;
    utc = static_cast<time_t>(seconds);
    return true;
}
void Clock::synchronize(time_t utc, uint32_t now) { utc_ = utc; last_ = now; remainder_ = 0; valid_ = true; }
void Clock::advance(uint32_t now) {
    if (!valid_) return;
    uint64_t elapsed = static_cast<uint32_t>(now - last_) + static_cast<uint64_t>(remainder_);
    utc_ += elapsed / 1000;
    remainder_ = elapsed % 1000;
    last_ = now;
}
tm Clock::local() const { tm value = {}; localtime_r(&utc_, &value); return value; }
int Clock::date() const { return valid_ ? dateOf(local()) : 0; }
void Row::receive(const Day& day, int currentDate) {
    if (!day.available) return; // Unknown source must not erase a useful display.
    if (currentDate == 0) { pending = day; hasPending = true; }
    else if (day.date == currentDate) { displayed = day; hasPending = false; }
}
void Row::activate(int currentDate) {
    if (hasPending && currentDate != 0) {
        if (pending.date == currentDate) displayed = pending;
        hasPending = false;
    }
}
uint32_t render(const Row& row, size_t position, const Clock& clock, bool night) {
    if (night || position >= LedsPerRow || !clock.valid() || !row.displayed.available) return 0;
    if (position == Hours) {
        return row.displayed.allDay == Slot::Important ? 0xff0000 :
               row.displayed.allDay == Slot::Normal ? 0xffffff : 0;
    }
    if (position > Hours) return 0;
    Slot slot = row.displayed.slots[position];
    uint32_t color = slot == Slot::Important ? 0xff0000 : slot == Slot::Normal ? 0xffffff : 0;
    if (row.displayed.date == clock.date()) {
        tm now = clock.local();
        if (slot != Slot::Empty && position < static_cast<size_t>(now.tm_hour)) color = 0x202020;
        if (position == static_cast<size_t>(now.tm_hour) && now.tm_sec % 2 == 0) color = 0x8b0000;
    }
    return color;
}
}
