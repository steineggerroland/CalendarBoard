#include <Arduino.h>
#include <FastLED_NeoPixel.h>
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
FastLED_NeoPixel_Variant strip(leds, NUM_LEDS);
int pixels_to_hsv[NUM_LEDS][3];

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

  strip.begin(FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS));
  strip_is_live_show();

  calendars[0] = BlinkyCalendar(0, "persons/roland/");
  calendars[1] = BlinkyCalendar(31, "persons/christina/");
  calendars[2] = BlinkyCalendar(62, "persons/nora/");
  calendars[3] = BlinkyCalendar(93, "persons/per/");

  setupMqtt(name, mqtt_host, mqtt_username, mqtt_password, messageHandler, connectedHandler);

  for (int i = 0; i < strip.numPixels(); i++) {
    resetLed(i);
    delay(50);
  }

  requestCalendarInformation();
}

void strip_is_live_show() {
  for (int i = 0; i < strip.numPixels(); i++) {
    leds[i] = CHSV(0, 0, 0);
  }
  FastLED.show();
  delay(50);
  for (int i = 0; i < strip.numPixels(); i++) {
    leds[i] = CHSV(0, 0, 10);
    FastLED.show();
    delay(50);
  }
  for (int i = 0; i < strip.numPixels(); i++) {
    leds[strip.numPixels() - 1 - i] = CHSV(0, 0, 0);
    FastLED.show();
    delay(50);
  }
}

void connectedHandler() {
  mqtt_subscribe("persons/#");
  mqtt_subscribe("home/things/" + name + "/nightmode");
}

void loop() {
  handleOta();
  handleMqtt();

  bool isLedUpdateInterval = millis() - millisLastUpdatedLeds > 1000;
  if (nightmode && isLedUpdateInterval) {
    for (int i = 0; i < strip.numPixels(); i++) {
      leds[i] = CHSV(0, 0, 0);
    }
    FastLED.show();
    millisLastUpdatedLeds = millis();
  } else if (isLedUpdateInterval) {
    for (int i = 0; i < strip.numPixels(); i++) {
      leds[i] = CHSV(pixels_to_hsv[i][0], pixels_to_hsv[i][1], min(50, pixels_to_hsv[i][2]));
    }
    FastLED.show();
    millisLastUpdatedLeds = millis();
  }

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
          pixels_to_hsv[stripIndex][0] = calendars[i].ledsForDay[ledIndex].hue;
          pixels_to_hsv[stripIndex][1] = calendars[i].ledsForDay[ledIndex].saturation;
          pixels_to_hsv[stripIndex][2] = calendars[i].ledsForDay[ledIndex].brightness;
        }
        delete[] appointments;
      }
    }
  }
  
  if (topic == "home/things/" + name + "/nightmode") {
    if (payload != "off") {
      nightmode = true;
      mqtt_unsubscribe("persons/#");
    }
    else {
      nightmode = false;
      mqtt_subscribe("persons/#");
      requestCalendarInformation();
    }
  }
}

void resetLed(int index) {
  pixels_to_hsv[index][0] = 0;
  pixels_to_hsv[index][1] = 0;
  pixels_to_hsv[index][2] = 0;
}

void requestCalendarInformation() {
  for (int i=NUM_CALENDARDS; i<NUM_CALENDARDS; i++) {
    mqtt_publish(calendars[i].mqttTopic + "calendar/request", "");
  }
}
