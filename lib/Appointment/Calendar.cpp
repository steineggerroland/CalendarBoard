#include "Calendar.h"

Time::Time(int h, int m) : hour(h), minute(m) {}

Appointment::Appointment(Time b, Time e, String t, CRGB c) : begin(b), end(e), title(t), color(c) {}

bool Appointment::isWithinTimeRange(Time startRange, Time endRange) {
  bool startsBeforeRangeEnds = (begin.hour < endRange.hour) || (begin.hour == endRange.hour && begin.minute < endRange.minute);
  bool endsAfterRangeStarts = (end.hour > startRange.hour) || (end.hour == startRange.hour && end.minute > startRange.minute);

  return startsBeforeRangeEnds && endsAfterRangeStarts;
}

Time parseTimeFromISO8601(String dateTime) {
  int hour = dateTime.substring(11, 13).toInt();
  int minute = dateTime.substring(14, 16).toInt();
  return Time(hour, minute);
}

Appointment* parseAppointments(JSONVar appointments, int count) {
  Appointment* parsedAppointments = new Appointment[count];

  for (int i = 0; i < count; i++) {
    JSONVar appointment = appointments[i];
    Time begin = parseTimeFromISO8601((const char*)appointment["begin"]);
    Time end = parseTimeFromISO8601((const char*)appointment["end"]);
    String title = (const char*)appointment["title"];

    parsedAppointments[i] = Appointment(begin, end, title, CHSV(int(rand() * 255), 100, 100));
  }

  return parsedAppointments;
}

int findAppointmentInRange(Appointment* appointments, int count, Time start, Time end);

BlinkyCalendar::BlinkyCalendar(int s, String t) : startIndex(s), mqttTopic(t), ledsForDay{CRGB::Black} {}
BlinkyCalendar::BlinkyCalendar() : startIndex(0), mqttTopic(""), ledsForDay{CRGB::Black} {}

void BlinkyCalendar::replaceAppointments(Appointment* appointments, int count) {
    for (int hour = 0; hour < 24; hour++) {
        int match_index = findAppointmentInRange(appointments, count, Time(hour, 0), Time(hour + 1, 0));
        if (match_index >= 0) {
            ledsForDay[hour] = appointments[match_index].color;
        } else {
            ledsForDay[hour] = CRGB::Black;
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
