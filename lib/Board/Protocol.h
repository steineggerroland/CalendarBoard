#pragma once
#include "Board.h"
namespace board {
constexpr size_t MaxPayload = 768;
bool decodeDay(const char* payload, const char* timezone, Day& day);
bool decodeTime(const char* payload, const char* timezone, time_t& utc);
}
