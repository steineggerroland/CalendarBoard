// Host adapter around the real firmware core. stdin carries received MQTT data.
#include "Board.h"
#include "Protocol.h"
#include <stdlib.h>
#include <iostream>
#include <string>
using namespace board;
int main() {
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); tzset();
    Clock clock; Row rows[4]; Synchronization<4> sync;
    bool night = false;
    uint32_t now = 0;
    std::string line;
    while (std::getline(std::cin, line)) {
        bool accepted = true, request = false;
        if (line.compare(0, 5, "time ") == 0) {
            time_t utc;
            accepted = decodeTime(line.c_str() + 5, "Europe/Berlin", utc);
            if (accepted) {
                clock.synchronize(utc, now); sync.receivedTime();
                for (auto& row : rows) row.activate(clock.date());
            }
        } else if (line.compare(0, 4, "day ") == 0 && line.size() > 6 && line[4] >= '0' && line[4] <= '3') {
            Day day; accepted = decodeDay(line.c_str() + 6, "Europe/Berlin", day);
            if (accepted) {
                size_t row = line[4] - '0'; rows[row].receive(day, clock.date()); sync.receivedDay(row, day.date);
            }
        } else if (line.compare(0, 8, "advance ") == 0) {
            now += static_cast<uint32_t>(strtoul(line.c_str() + 8, nullptr, 10)); clock.advance(now);
        } else if (line == "night on") night = true;
        else if (line == "night off") night = false;
        else if (line == "request") { request = sync.due(now, clock.date()); if (request) sync.attempted(now); }
        else if (line == "reconnect") sync.connected();
        else if (line != "state") accepted = false;
        std::cout << "{\"accepted\":" << (accepted ? "true" : "false") << ",\"date\":" << clock.date()
                  << ",\"request\":" << (request ? "true" : "false") << ",\"colors\":[";
        for (size_t r = 0; r < 4; ++r) {
            if (r) std::cout << ',';
            std::cout << '[';
            for (size_t p = 0; p < LedsPerRow; ++p) {
                if (p) std::cout << ',';
                std::cout << render(rows[r], p, clock, night);
            }
            std::cout << ']';
        }
        std::cout << "]}" << std::endl;
    }
}
