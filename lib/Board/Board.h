#pragma once
#include <stddef.h>
#include <stdint.h>
#include <time.h>

namespace board {
constexpr size_t Hours = 24;
constexpr size_t LedsPerRow = 31;
enum class Slot : uint8_t { Empty, Normal, Important };
struct Day {
    int date = 0; // YYYYMMDD, local date
    Slot slots[Hours] = {};
    bool available = false;
    bool stale = false;
};
struct Row {
    Day displayed;
    Day pending;
    bool hasPending = false;
    void receive(const Day& day, int currentDate);
    void activate(int currentDate);
};
class Clock {
public:
    bool valid() const { return valid_; }
    void synchronize(time_t utc, uint32_t now);
    void advance(uint32_t now);
    tm local() const;
    int date() const;
    time_t utc() const { return utc_; }
private:
    bool valid_ = false;
    time_t utc_ = 0;
    uint32_t last_ = 0;
    uint32_t remainder_ = 0;
};
int dateOf(const tm& value);
bool parseDate(const char* text, int& date);
bool parseTimestamp(const char* text, time_t& utc);
uint32_t render(const Row& row, size_t position, const Clock& clock, bool night);
constexpr size_t stripIndex(size_t row, size_t position) { return row * LedsPerRow + position; }
}
