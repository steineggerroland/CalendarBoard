#include <Arduino.h>
#include <FastLED.h>
#include <Arduino_JSON.h> // supplies the cJSON implementation used by Protocol
#include <Board.h>
#include <Protocol.h>
#include <BoardConfig.h>
#include <WlanHelper.h>
#include <MqttHelper.h>
#include "secrets.h"

namespace {
const String boardName = CALENDAR_BOARD_NAME;
const String baseTopic = "calendarboard/v1/" + boardName;
constexpr size_t LedCount = settings::RowCount * board::LedsPerRow;
board::Row rows[settings::RowCount];
board::Clock boardClock;
CRGB leds[LedCount];
bool nightMode = false;
bool frameInitialized = false;
uint32_t lastFrame = 0;
uint32_t lastHeartbeat = 0;
board::Synchronization<settings::RowCount> synchronization;
bool configured = false;

bool validId(const char* id) {
    size_t length = strlen(id);
    if (length == 0 || length > 32) return false;
    for (size_t i = 0; i < length; ++i)
        if (!((id[i] >= 'a' && id[i] <= 'z') || (id[i] >= 'A' && id[i] <= 'Z') ||
              (id[i] >= '0' && id[i] <= '9') || id[i] == '-' || id[i] == '_')) return false;
    return true;
}
bool configurationValid() {
    if (!validId(boardName.c_str())) return false;
    for (size_t row = 0; row < settings::RowCount; ++row) {
        if (!validId(settings::RowIds[row])) return false;
        for (size_t previous = 0; previous < row; ++previous)
            if (strcmp(settings::RowIds[previous], settings::RowIds[row]) == 0) return false;
    }
    return true;
}
void connected() {
    bool subscribed = mqtt_subscribe(baseTopic + "/time");
    for (size_t row = 0; row < settings::RowCount; ++row)
        subscribed = mqtt_subscribe(baseTopic + "/rows/" + settings::RowIds[row] + "/day") && subscribed;
    subscribed = mqtt_subscribe("home/things/" + boardName + "/nightmode") && subscribed;
    if (!subscribed) {
        Serial.println("MQTT subscription failed; reconnecting");
        mqtt_disconnect();
    }
    synchronization.connected();
}
void receive(String& topic, String& payload) {
    if (payload.length() > board::MaxPayload || strlen(payload.c_str()) != payload.length()) {
        Serial.println("Rejected oversized or binary payload");
        return;
    }
    if (topic == "home/things/" + boardName + "/nightmode") {
        if (payload == "on") nightMode = true;
        else if (payload == "off") nightMode = false;
        else Serial.println("Rejected nightmode value");
        return;
    }
    if (topic == baseTopic + "/time") {
        time_t utc;
        if (!board::decodeTime(payload.c_str(), settings::Timezone, utc)) {
            Serial.println("Rejected time message");
            return;
        }
        boardClock.synchronize(utc, millis());
        synchronization.receivedTime();
        for (size_t row = 0; row < settings::RowCount; ++row) rows[row].activate(boardClock.date());
        return;
    }
    for (size_t row = 0; row < settings::RowCount; ++row) {
        if (topic != baseTopic + "/rows/" + settings::RowIds[row] + "/day") continue;
        board::Day day;
        if (!board::decodeDay(payload.c_str(), settings::Timezone, day)) {
            Serial.println("Rejected day message");
            return;
        }
        rows[row].receive(day, boardClock.date());
        synchronization.receivedDay(row, day.date);
        return;
    }
}
void synchronize(uint32_t now) {
    if (mqtt_connected() && synchronization.due(now, boardClock.date())) {
        // Publish outside the inbound MQTT callback to avoid reentrant client operations.
        if (!mqtt_publish(baseTopic + "/sync/request", "{\"schema_version\":1}"))
            Serial.println("Synchronization request failed; retry scheduled");
        synchronization.attempted(now);
    }
}

void render(uint32_t now) {
    if (frameInitialized && static_cast<uint32_t>(now - lastFrame) < 200) return;
    bool changed = !frameInitialized;
    for (size_t row = 0; row < settings::RowCount; ++row) {
        for (size_t position = 0; position < board::LedsPerRow; ++position) {
            size_t index = board::stripIndex(row, position);
            CRGB color(board::render(rows[row], position, boardClock, nightMode));
            if (leds[index] != color) { leds[index] = color; changed = true; }
        }
    }
    if (changed) FastLED.show();
    frameInitialized = true;
    lastFrame = now;
}
}

void setup() {
    Serial.begin(9600);
    setTZ(settings::TimezoneRule);
    FastLED.addLeds<WS2812B, settings::DataPin, GRB>(leds, LedCount);
    FastLED.setCorrection(CRGB(210, 255, 150));
    render(millis());
    configured = configurationValid();
    if (!configured) {
        Serial.println("Invalid board or row IDs; fix BoardConfig.h");
        return;
    }
    connectWlan(boardName, SECRET_SSID, SECRET_PASS, OTA_PASS);
    setupMqtt(boardName, MQTT_HOST, SECRET_MQTT_USER, SECRET_MQTT_PASS, receive, connected);
}

void loop() {
    handleOta();
    if (!configured) { yield(); return; }
    // Advance before receiving new day data, including a local midnight crossed in this loop.
    boardClock.advance(millis());
    handleMqtt();
    uint32_t now = millis();
    boardClock.advance(now);
    synchronize(now);
    render(now);
    if (static_cast<uint32_t>(now - lastHeartbeat) >= 25000 && mqtt_connected()) {
        mqtt_publish("home/things/" + boardName + "/state",
                     "{\"online_status\":\"online\",\"ip\":\"" + getIp() + "\"}");
        lastHeartbeat = now;
    }
}
