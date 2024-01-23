#include "Appointment.h"

Time::Time(int h, int m) : hour(h), minute(m) {}

Color::Color(int h, int s, int b) : hue(h), saturation(s), brightness(b) {}

Appointment::Appointment(Time b, Time e, String t, Color c) : begin(b), end(e), title(t), color(c) {}

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

Appointment* parseAppointments(JSONVar appointments, int& count) {
  count = appointments.length();
  Appointment* parsedAppointments = new Appointment[count];

  for (int i = 0; i < count; i++) {
    JSONVar appointment = appointments[i];
    Time begin = parseTimeFromISO8601((const char*)appointment["begin"]);
    Time end = parseTimeFromISO8601((const char*)appointment["end"]);
    String title = (const char*)appointment["title"];

    parsedAppointments[i] = Appointment(begin, end, title, Color(int(rand() * 255), 255, 255));
  }

  return parsedAppointments;
}
