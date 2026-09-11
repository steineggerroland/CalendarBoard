#pragma once
#include <stddef.h>

// Stable IDs must match VirtualEntities calendar_boards[].rows[].id, in strip order.
namespace settings {
constexpr const char* RowIds[] = {"person1", "person2", "person3", "person4"};
constexpr size_t RowCount = sizeof(RowIds) / sizeof(RowIds[0]);
constexpr const char* Timezone = "Europe/Berlin";
// POSIX rule corresponding to the IANA name above. Used while disconnected too.
constexpr const char* TimezoneRule = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr int DataPin = 4;
static_assert(RowCount > 0, "Configure at least one row");
}
