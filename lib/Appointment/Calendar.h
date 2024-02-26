#include <Arduino.h>
#include <FastLED.h>
#include <Arduino_JSON.h>

class Time {
public:
  int hour;
  int minute;

  Time(int h, int m);
};

class Appointment {
public:
  Time begin;
  Time end;
  String title;
  CRGB color;

  Appointment(Time b, Time e, String t, CRGB c);
  Appointment() : begin(0, 0), end(0, 0), title(""), color(CRGB::Black) {};

  bool isWithinTimeRange(Time s, Time e);
};

Time parseTimeFromISO8601(String dateTime);
Appointment* parseAppointments(JSONVar appointments, int count);


class BlinkyCalendar {
public:
    int startIndex;
    String mqttTopic;
    CRGB ledsForDay[24];

    BlinkyCalendar(int startLed, String mqttTopic);
    BlinkyCalendar();

    void replaceAppointments(Appointment* appointments, int count);
};
