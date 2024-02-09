#include <Arduino.h>
#include <FastLED_NeoPixel.h>
#include <Arduino_JSON.h>

class Time {
public:
  int hour;
  int minute;

  Time(int h, int m);
};

class ColorHSV {
public:
  int hue;
  int saturation;
  int brightness;

  ColorHSV(int h, int s, int b);
  ColorHSV() : hue(0), saturation(0), brightness(0) {};
};

class Appointment {
public:
  Time begin;
  Time end;
  String title;
  ColorHSV color;

  Appointment(Time b, Time e, String t, ColorHSV c);
  Appointment() : begin(0, 0), end(0, 0), title(""), color(0, 0, 0) {};

  bool isWithinTimeRange(Time s, Time e);
};

Time parseTimeFromISO8601(String dateTime);
Appointment* parseAppointments(JSONVar appointments, int count);


class BlinkyCalendar {
public:
    int startIndex;
    String mqttTopic;
    ColorHSV ledsForDay[24];

    BlinkyCalendar(int startLed, String mqttTopic);
    BlinkyCalendar();

    void replaceAppointments(Appointment* appointments, int count);
};
