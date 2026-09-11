#include "Protocol.h"
#include <cjson/cJSON.h>
#include <string.h>

namespace board {
namespace {
struct Document {
    cJSON* root = nullptr;
    explicit Document(const char* payload) {
        if (payload && strlen(payload) <= MaxPayload) root = cJSON_ParseWithOpts(payload, nullptr, 1);
    }
    ~Document() { cJSON_Delete(root); }
};
const cJSON* field(const cJSON* root, const char* name) { return cJSON_GetObjectItemCaseSensitive(root, name); }
const char* stringField(const cJSON* root, const char* name) {
    const cJSON* item = field(root, name);
    return cJSON_IsString(item) ? item->valuestring : nullptr;
}
bool equals(const char* a, const char* b) { return a && b && strcmp(a, b) == 0; }
bool envelope(const cJSON* root, const char* zone) {
    if (!cJSON_IsObject(root)) return false;
    for (const cJSON* a = root->child; a; a = a->next)
        for (const cJSON* b = a->next; b; b = b->next)
            if (equals(a->string, b->string)) return false;
    const cJSON* version = field(root, "schema_version");
    return cJSON_IsNumber(version) && version->valuedouble == 1 && equals(stringField(root, "timezone"), zone);
}
}
bool decodeDay(const char* payload, const char* timezone, Day& day) {
    Document doc(payload);
    if (!envelope(doc.root, timezone)) return false;
    Day candidate;
    time_t generated, checked;
    if (!parseDate(stringField(doc.root, "date"), candidate.date) ||
        !parseTimestamp(stringField(doc.root, "generated_at"), generated)) return false;
    const char* status = stringField(doc.root, "status");
    const cJSON* slots = field(doc.root, "slots");
    const cJSON* source = field(doc.root, "source_checked_at");
    if (!source || (!cJSON_IsNull(source) && !parseTimestamp(stringField(doc.root, "source_checked_at"), checked))) return false;
    if (equals(status, "unavailable")) {
        if (!cJSON_IsNull(slots)) return false;
    } else {
        if ((!equals(status, "ok") && !equals(status, "stale")) || cJSON_IsNull(source) ||
            !cJSON_IsArray(slots) || cJSON_GetArraySize(slots) != Hours) return false;
        candidate.available = true;
        candidate.stale = equals(status, "stale");
        for (size_t hour = 0; hour < Hours; ++hour) {
            const cJSON* slot = cJSON_GetArrayItem(slots, hour);
            if (cJSON_IsNull(slot)) candidate.slots[hour] = Slot::Empty;
            else if (cJSON_IsString(slot) && equals(slot->valuestring, "ffffff")) candidate.slots[hour] = Slot::Normal;
            else if (cJSON_IsString(slot) && equals(slot->valuestring, "ff0000")) candidate.slots[hour] = Slot::Important;
            else return false;
        }
    }
    day = candidate;
    return true;
}
bool decodeTime(const char* payload, const char* timezone, time_t& utc) {
    Document doc(payload);
    if (!envelope(doc.root, timezone)) return false;
    time_t candidate, local;
    const char* localText = stringField(doc.root, "local");
    if (!parseTimestamp(stringField(doc.root, "utc"), candidate) ||
        !parseTimestamp(localText, local) || candidate != local || strlen(localText) != 25) return false;
    tm localValue; localtime_r(&candidate, &localValue);
    char expected[20]; strftime(expected, sizeof(expected), "%Y-%m-%dT%H:%M:%S", &localValue);
    if (strncmp(expected, localText, 19) != 0) return false;
    utc = candidate;
    return true;
}
}
