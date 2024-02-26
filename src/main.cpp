#include <Arduino.h>
#include <FastLED.h>
#include <Arduino_JSON.h>

#include <Calendar.h>
#include <WlanHelper.h>
#include <MqttHelper.h>
#include "secrets.h"

#define DATA_PIN 4
#define NUM_LEDS 124
#define NUM_CALENDARDS 4

const String name = "calendar-board-1";
const String mqtt_host = "openhabian.lan";
const String mqtt_username = SECRET_MQTT_USER;
const String mqtt_password = SECRET_MQTT_PASS;
const String wlan_ssid = SECRET_SSID;
const String wlan_password = SECRET_PASS;
const String ota_password = OTA_PASS;

BlinkyCalendar calendars[NUM_CALENDARDS]; // initialized in setup()

CRGB leds[NUM_LEDS];

unsigned long millisLastMessageSent = 0;
unsigned long millisLastUpdatedLeds = 0;
bool nightmode = false;

void resetLed(int index);

void messageHandler(String& topic, String& payload);

void connectedHandler();

void strip_is_live_show();

void requestCalendarInformation();

void setup() {
  Serial.begin(9600);
  Serial.println("Booting");
  connectWlan(name, wlan_ssid, wlan_password, ota_password);

  FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS);
  FastLED.setMaxRefreshRate(400);
  FastLED.setBrightness(30);
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  FastLED.show();

  calendars[0] = BlinkyCalendar(0, "persons/roland/");
  calendars[1] = BlinkyCalendar(31, "persons/christina/");
  calendars[2] = BlinkyCalendar(62, "persons/nora/");
  calendars[3] = BlinkyCalendar(93, "persons/per/");

  setupMqtt(name, mqtt_host, mqtt_username, mqtt_password, messageHandler, connectedHandler);

  strip_is_live_show();
}

void strip_is_live_show() {
  for (int step = 0; step < 32; step++) {
    for (int cal = 0; cal < NUM_CALENDARDS; cal++) {
      int offset = cal * 31;
      if (step == 0) {
        leds[15 + offset] = CRGB::DarkBlue;
      } else if (step <= 15) {
        leds[15 - step + offset] = CRGB::DarkBlue;
        leds[15 + step + offset] = CRGB::DarkBlue;
      } else if (step == 16) {
        leds[15 + offset] = CRGB::Black;
      } else {
        int internal_step = step - 16;
        leds[15 - internal_step + offset] = CRGB::Black;
        leds[15 + internal_step + offset] = CRGB::Black;
      }
    }
    delay(100);
    FastLED.show();
  }
}

void connectedHandler() {
  mqtt_subscribe("persons/#");
  mqtt_subscribe("home/things/" + name + "/nightmode");
  requestCalendarInformation();
}

void loop() {
  handleOta();
  handleMqtt();

  if (millis() - millisLastMessageSent > 25000) {
    mqtt_publish("home/things/" + name + "/state", "{\"online_status\":\"online\"}");
    millisLastMessageSent = millis();
  }
}

void messageHandler(String& topic, String& payload) {
  Serial.println("incoming: " + topic + " - " + payload);

  for (int i = 0; i < NUM_CALENDARDS; i++) {
    if (topic == calendars[i].mqttTopic + "appointments") {
      JSONVar request = JSON.parse(payload);
      if (request.hasOwnProperty("appointments")) {
        int count = request["appointments"].length();
        Appointment* appointments = parseAppointments(request["appointments"], count);
        calendars[i].replaceAppointments(appointments, count);
        for (int ledIndex = 0; ledIndex < 24; ledIndex++) {
          int stripIndex = calendars[i].startIndex + ledIndex;
          leds[stripIndex] = calendars[i].ledsForDay[ledIndex];
        }
        delete[] appointments;
      }
      FastLED.show();
    }
  }
  
  if (topic == "home/things/" + name + "/nightmode") {
    if (payload != "off") {
      nightmode = true;
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
      }
      FastLED.show();
      mqtt_unsubscribe("persons/#");
    }
    else {
      nightmode = false;
      mqtt_subscribe("persons/#");
      requestCalendarInformation();
    }
  }
}

void requestCalendarInformation() {
  for (int i=0; i<NUM_CALENDARDS; i++) {
    mqtt_publish(calendars[i].mqttTopic + "calendar/request", "");
  }
}
