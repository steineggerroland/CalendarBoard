#include <Arduino.h>
#include <FastLED_NeoPixel.h>
#include <Arduino_JSON.h>

#include <Appointment.h>
#include <WlanHelper.h>
#include <MqttHelper.h>
#include "secrets.h"

#define DATA_PIN 4
#define NUM_LEDS 124

const String name = "calendar-board-1";
const String mqtt_host = "openhabian.lan";
const String mqtt_username = SECRET_MQTT_USER;
const String mqtt_password = SECRET_MQTT_PASS;
const String wlan_ssid = SECRET_SSID;
const String wlan_password = SECRET_PASS;
const String ota_password = OTA_PASS;


CRGB leds[NUM_LEDS];
FastLED_NeoPixel_Variant strip(leds, NUM_LEDS);
int pixels_to_hsv[NUM_LEDS][3];

unsigned long millisLastMessageSent = 0;
unsigned long millisLastUpdatedLeds = 0;
bool nightmode = false;

int findAppointmentInRange(Appointment* appointments, int count, Time start, Time end);

void resetLed(int index);

void adaptHoursToAppointments(Appointment* appointments, int count);

void messageHandler(String& topic, String& payload);

void strip_is_live_show();

void setup() {
  Serial.begin(9600);
  Serial.println("Booting");
  connectWlan(name, wlan_ssid, wlan_password, ota_password);

  strip.begin(FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS));
  strip_is_live_show();

  setupMqtt(name, mqtt_host, mqtt_username, mqtt_password, messageHandler);

  for (int i = 0; i < strip.numPixels(); i++) {
    resetLed(i);
    delay(50);
  }

  mqtt_publish("persons/roland/calendar/request", "");
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

void loop() {
  handleOta();
  handleMqtt();

  bool isLedUpdateInterval = millis() - millisLastUpdatedLeds > 100;
  if (nightmode && isLedUpdateInterval) {
    for (int i = 0; i < strip.numPixels(); i++) {
      leds[i] = CHSV(0, 0, 0);
    }
    FastLED.show();
    millisLastUpdatedLeds = millis();
  }
  else if (isLedUpdateInterval) {
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

  if (topic == "persons/roland/appointments") {
    JSONVar request = JSON.parse(payload);
    if (request.hasOwnProperty("appointments")) {
      int count = 0;
      Appointment* appointments = parseAppointments(request["appointments"], count);
      adaptHoursToAppointments(appointments, count);
      delete[] appointments;
    }
  }
  else if (topic == "home/things/" + name + "/nightmode") {
    if (payload != "off") {
      nightmode = true;
      mqtt_unsubscribe("persons/roland/appointments");
    }
    else {
      nightmode = false;
      mqtt_subscribe("persons/roland/appointments");
    }
  }
}

void adaptHoursToAppointments(Appointment* appointments, int count) {
  for (int hour = 0; hour < 24; hour++) {
    int match_index = findAppointmentInRange(appointments, count, Time(hour, 0), Time(hour + 1, 0));
    if (match_index >= 0) {
      pixels_to_hsv[hour][0] = appointments[match_index].color.hue;
      pixels_to_hsv[hour][1] = appointments[match_index].color.saturation;
      pixels_to_hsv[hour][2] = appointments[match_index].color.brightness;
    }
    else {
      resetLed(hour);
    }
  }
}

int findAppointmentInRange(Appointment* appointments, int count, Time start, Time end) {
  for (int appointment_index = 0; appointment_index < count; appointment_index++) {
    if (appointments[appointment_index].isWithinTimeRange(start, end)) {
      return appointment_index;
    }
  }
  return -1;
}

void resetLed(int index) {
  pixels_to_hsv[index][0] = 0;
  pixels_to_hsv[index][1] = 0;
  pixels_to_hsv[index][2] = 0;
}
