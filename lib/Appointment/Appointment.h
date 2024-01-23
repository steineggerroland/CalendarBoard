#include <Arduino.h>
#include <Arduino_JSON.h>

class Time {
public:
  int hour;
  int minute;

  Time(int h, int m);
};

class Color {
public:
  int hue;
  int saturation;
  int brightness;

  Color(int h, int s, int b);
};

class Appointment {
public:
  Time begin;
  Time end;
  String title;
  Color color;

  Appointment(Time b, Time e, String t, Color c);
  Appointment() : begin(0, 0), end(0, 0), title(""), color(0, 0, 0) {}

  bool isWithinTimeRange(Time s, Time e);
};

Time parseTimeFromISO8601(String dateTime);
Appointment* parseAppointments(JSONVar json, int& count);
