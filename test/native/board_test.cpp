#include "Board.h"
#include "Protocol.h"
#include <assert.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <iterator>
#include <iostream>
using namespace board;
static std::string read(const char* path) { std::ifstream f(path); assert(f); return {std::istreambuf_iterator<char>(f), {}}; }
static time_t stamp(const char* text) { time_t t; assert(parseTimestamp(text, t)); return t; }
int main() {
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); tzset();
    int date;
    assert(!parseDate("2026-02-29", date)); assert(parseDate("2028-02-29", date));
    time_t t;
    assert(!parseTimestamp("2026-09-11T24:00:00Z", t));
    assert(!parseTimestamp("2026-09-11T14:00:00+14:30", t));
    assert(stamp("2026-09-11T14:00:00+02:00") == stamp("2026-09-11T12:00:00Z"));
    Synchronization<2> sync;
    assert(sync.due(0, 0)); sync.attempted(0);
    assert(!sync.due(4999, 0)); assert(sync.due(5000, 0)); sync.attempted(5000);
    assert(!sync.due(34999, 0)); assert(sync.due(35000, 0));
    sync.receivedTime(); sync.receivedDay(0, 20260911);
    assert(sync.due(35000, 20260911)); sync.attempted(35000);
    assert(sync.due(40000, 20260911)); // one missing row
    sync.receivedDay(1, 20260911); assert(!sync.due(100000, 20260911));
    assert(sync.due(100001, 20260912)); sync.attempted(100001);
    sync.receivedDay(0, 20260912); sync.receivedDay(1, 20260912);
    assert(!sync.due(200000, 20260912));
    sync.connected(); assert(sync.due(200000, 20260912));
    Clock clock;
    clock.synchronize(stamp("2026-09-10T22:59:59Z"), 0xfffffff0);
    clock.advance(0x7c0); // rollover, 2000ms
    assert(clock.local().tm_hour == 1 && clock.local().tm_sec == 1);
    clock.advance(0x7c0 + 1500); clock.advance(0x7c0 + 2000);
    assert(clock.local().tm_sec == 3);
    clock.synchronize(stamp("2026-03-29T00:59:59Z"), 0); clock.advance(1000);
    assert(clock.local().tm_hour == 3);
    clock.synchronize(stamp("2026-10-25T00:59:59Z"), 0); clock.advance(1000);
    assert(clock.local().tm_hour == 2 && clock.local().tm_isdst == 0);
    clock.synchronize(stamp("2026-09-11T21:59:59Z"), 0); clock.advance(1000);
    assert(clock.date() == 20260912);
    auto payload = read("test/fixtures/day.json");
    Day day; assert(decodeDay(payload.c_str(), "Europe/Berlin", day));
    assert(day.available && day.slots[14] == Slot::Important);
    assert(day.allDay == Slot::Empty);
    auto withAllDay = payload;
    withAllDay.replace(withAllDay.find("\"all_day\":null"), 14, "\"all_day\":\"ff0000\"");
    Day dayWithAllDay; assert(decodeDay(withAllDay.c_str(), "Europe/Berlin", dayWithAllDay));
    assert(dayWithAllDay.allDay == Slot::Important);
    auto invalidAllDay = withAllDay;
    invalidAllDay.replace(invalidAllDay.find("\"all_day\":\"") + 11, 6, "00ff00");
    assert(!decodeDay(invalidAllDay.c_str(), "Europe/Berlin", dayWithAllDay));
    assert(!decodeDay(payload.c_str(), "UTC", day));
    auto invalid = payload; invalid.replace(invalid.find("ff0000"), 6, "00ff00");
    assert(!decodeDay(invalid.c_str(), "Europe/Berlin", day));
    assert(day.slots[14] == Slot::Important); // failed parsing did not mutate
    assert(!decodeDay((payload + "junk").c_str(), "Europe/Berlin", day));
    assert(!decodeDay(std::string(769, ' ').c_str(), "Europe/Berlin", day));
    auto duplicate = payload;
    duplicate.insert(1, "\"schema_version\":1,");
    assert(!decodeDay(duplicate.c_str(), "Europe/Berlin", day));
    auto shortSlots = payload; shortSlots.erase(shortSlots.find("null,"), 5);
    assert(!decodeDay(shortSlots.c_str(), "Europe/Berlin", day));
    Day unknown;
    assert(decodeDay("{\"schema_version\":1,\"timezone\":\"Europe/Berlin\",\"date\":\"2026-09-11\","
                     "\"generated_at\":\"2026-09-11T12:00:00Z\",\"source_checked_at\":null,"
                     "\"status\":\"unavailable\",\"slots\":null}", "Europe/Berlin", unknown));
    assert(!unknown.available);
    assert(!decodeDay("{\"schema_version\":1,\"timezone\":\"Europe/Berlin\",\"date\":\"2026-09-11\","
                      "\"generated_at\":\"2026-09-11T12:00:00Z\",\"source_checked_at\":null,"
                      "\"status\":\"unavailable\",\"all_day\":\"00ff00\",\"slots\":null}", "Europe/Berlin", unknown));
    auto time = read("test/fixtures/time.json");
    assert(decodeTime(time.c_str(), "Europe/Berlin", t));
    auto badTime = time; badTime.replace(badTime.find("14:00:00"), 8, "13:00:00");
    assert(!decodeTime(badTime.c_str(), "Europe/Berlin", t));
    Row row; row.receive(day, 0); assert(!row.displayed.available);
    clock.synchronize(t, 0); row.activate(clock.date());
    assert(row.displayed.available);
    assert(render(row, 14, clock, false) == 0x5a4030);
    clock.advance(4000); assert(render(row, 14, clock, false) == 0xff0000);
    assert(render(row, 14, clock, true) == 0);
    row.receive(day, clock.date()); assert(render(row, 14, clock, false) == 0xff0000);
    clock.advance(3601000); assert(render(row, 14, clock, false) == 0x202020);
    row.receive(dayWithAllDay, clock.date());
    assert(render(row, Hours, clock, false) == 0xff0000);
    assert(render(row, Hours, clock, true) == 0);
    Day unavailable; unavailable.date = day.date; row.receive(unavailable, day.date);
    assert(row.displayed.available);
    Day old = day; old.date--; row.receive(old, day.date); assert(row.displayed.date == day.date);
    for (size_t rows : {1u, 4u, 6u})
        for (size_t r = 0; r < rows; ++r)
            for (size_t position = 0; position < LedsPerRow; ++position) {
                assert(stripIndex(r, position) < rows * LedsPerRow);
                if (position > Hours) assert(render(row, position, clock, false) == 0);
            }
    clock.synchronize(stamp("2026-09-12T12:00:00Z"), 0);
    assert(render(row, 14, clock, false) == 0xff0000); // retain old day without today's overlays
    std::cout << "Board protocol, rendering, clock and layout checks passed\n";
}
