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
// Per-row replies prevent a valid clock from hiding missing day snapshots.
template<size_t RowCount>
class Synchronization {
public:
    void connected() {
        immediate_ = true;
        attempts_ = 0;
        timeReceived_ = false;
        for (size_t row = 0; row < RowCount; ++row) replies_[row] = 0;
    }
    void receivedTime() { timeReceived_ = true; }
    void receivedDay(size_t row, int date) { if (row < RowCount) replies_[row] = date; }
    bool due(uint32_t now, int date) {
        if (date != date_) { date_ = date; immediate_ = true; attempts_ = 0; }
        bool complete = timeReceived_ && date != 0;
        for (size_t row = 0; row < RowCount; ++row) complete = complete && replies_[row] == date;
        uint32_t interval = attempts_ < 2 ? 5000 : 30000;
        return immediate_ || (!complete && static_cast<uint32_t>(now - last_) >= interval);
    }
    void attempted(uint32_t now) {
        last_ = now;
        if (attempts_ < 2) ++attempts_;
        immediate_ = false;
    }
private:
    bool immediate_ = true;
    bool timeReceived_ = false;
    unsigned attempts_ = 0;
    uint32_t last_ = 0;
    int date_ = 0;
    int replies_[RowCount] = {};
};
int dateOf(const tm& value);
bool parseDate(const char* text, int& date);
bool parseTimestamp(const char* text, time_t& utc);
uint32_t render(const Row& row, size_t position, const Clock& clock, bool night);
constexpr size_t stripIndex(size_t row, size_t position) { return row * LedsPerRow + position; }
}
