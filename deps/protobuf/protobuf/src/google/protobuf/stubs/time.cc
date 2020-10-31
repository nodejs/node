#include <google/protobuf/stubs/time.h>

#include <ctime>

#include <google/protobuf/stubs/stringprintf.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace internal {

namespace {
static const int64 kSecondsPerMinute = 60;
static const int64 kSecondsPerHour = 3600;
static const int64 kSecondsPerDay = kSecondsPerHour * 24;
static const int64 kSecondsPer400Years =
    kSecondsPerDay * (400 * 365 + 400 / 4 - 3);
// Seconds from 0001-01-01T00:00:00 to 1970-01-01T:00:00:00
static const int64 kSecondsFromEraToEpoch = 62135596800LL;
// The range of timestamp values we support.
static const int64 kMinTime = -62135596800LL;  // 0001-01-01T00:00:00
static const int64 kMaxTime = 253402300799LL;  // 9999-12-31T23:59:59

static const int kNanosPerMillisecond = 1000000;
static const int kNanosPerMicrosecond = 1000;

// Count the seconds from the given year (start at Jan 1, 00:00) to 100 years
// after.
int64 SecondsPer100Years(int year) {
  if (year % 400 == 0 || year % 400 > 300) {
    return kSecondsPerDay * (100 * 365 + 100 / 4);
  } else {
    return kSecondsPerDay * (100 * 365 + 100 / 4 - 1);
  }
}

// Count the seconds from the given year (start at Jan 1, 00:00) to 4 years
// after.
int64 SecondsPer4Years(int year) {
  if ((year % 100 == 0 || year % 100 > 96) &&
      !(year % 400 == 0 || year % 400 > 396)) {
    // No leap years.
    return kSecondsPerDay * (4 * 365);
  } else {
    // One leap years.
    return kSecondsPerDay * (4 * 365 + 1);
  }
}

bool IsLeapYear(int year) {
  return year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
}

int64 SecondsPerYear(int year) {
  return kSecondsPerDay * (IsLeapYear(year) ? 366 : 365);
}

static const int kDaysInMonth[13] = {
  0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

int64 SecondsPerMonth(int month, bool leap) {
  if (month == 2 && leap) {
    return kSecondsPerDay * (kDaysInMonth[month] + 1);
  }
  return kSecondsPerDay * kDaysInMonth[month];
}

static const int kDaysSinceJan[13] = {
  0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334,
};

bool ValidateDateTime(const DateTime& time) {
  if (time.year < 1 || time.year > 9999 ||
      time.month < 1 || time.month > 12 ||
      time.day < 1 || time.day > 31 ||
      time.hour < 0 || time.hour > 23 ||
      time.minute < 0 || time.minute > 59 ||
      time.second < 0 || time.second > 59) {
    return false;
  }
  if (time.month == 2 && IsLeapYear(time.year)) {
    return time.day <= kDaysInMonth[time.month] + 1;
  } else {
    return time.day <= kDaysInMonth[time.month];
  }
}

// Count the number of seconds elapsed from 0001-01-01T00:00:00 to the given
// time.
int64 SecondsSinceCommonEra(const DateTime& time) {
  int64 result = 0;
  // Years should be between 1 and 9999.
  assert(time.year >= 1 && time.year <= 9999);
  int year = 1;
  if ((time.year - year) >= 400) {
    int count_400years = (time.year - year) / 400;
    result += kSecondsPer400Years * count_400years;
    year += count_400years * 400;
  }
  while ((time.year - year) >= 100) {
    result += SecondsPer100Years(year);
    year += 100;
  }
  while ((time.year - year) >= 4) {
    result += SecondsPer4Years(year);
    year += 4;
  }
  while (time.year > year) {
    result += SecondsPerYear(year);
    ++year;
  }
  // Months should be between 1 and 12.
  assert(time.month >= 1 && time.month <= 12);
  int month = time.month;
  result += kSecondsPerDay * kDaysSinceJan[month];
  if (month > 2 && IsLeapYear(year)) {
    result += kSecondsPerDay;
  }
  assert(time.day >= 1 &&
         time.day <= (month == 2 && IsLeapYear(year)
                          ? kDaysInMonth[month] + 1
                          : kDaysInMonth[month]));
  result += kSecondsPerDay * (time.day - 1);
  result += kSecondsPerHour * time.hour +
      kSecondsPerMinute * time.minute +
      time.second;
  return result;
}

// Format nanoseconds with either 3, 6, or 9 digits depending on the required
// precision to represent the exact value.
string FormatNanos(int32 nanos) {
  if (nanos % kNanosPerMillisecond == 0) {
    return StringPrintf("%03d", nanos / kNanosPerMillisecond);
  } else if (nanos % kNanosPerMicrosecond == 0) {
    return StringPrintf("%06d", nanos / kNanosPerMicrosecond);
  } else {
    return StringPrintf("%09d", nanos);
  }
}

// Parses an integer from a null-terminated char sequence. The method
// consumes at most "width" chars. Returns a pointer after the consumed
// integer, or nullptr if the data does not start with an integer or the
// integer value does not fall in the range of [min_value, max_value].
const char* ParseInt(const char* data, int width, int min_value,
                     int max_value, int* result) {
  if (!ascii_isdigit(*data)) {
    return nullptr;
  }
  int value = 0;
  for (int i = 0; i < width; ++i, ++data) {
    if (ascii_isdigit(*data)) {
      value = value * 10 + (*data - '0');
    } else {
      break;
    }
  }
  if (value >= min_value && value <= max_value) {
    *result = value;
    return data;
  } else {
    return nullptr;
  }
}

// Consumes the fractional parts of a second into nanos. For example,
// "010" will be parsed to 10000000 nanos.
const char* ParseNanos(const char* data, int32* nanos) {
  if (!ascii_isdigit(*data)) {
    return nullptr;
  }
  int value = 0;
  int len = 0;
  // Consume as many digits as there are but only take the first 9 into
  // account.
  while (ascii_isdigit(*data)) {
    if (len < 9) {
      value = value * 10 + *data - '0';
    }
    ++len;
    ++data;
  }
  while (len < 9) {
    value = value * 10;
    ++len;
  }
  *nanos = value;
  return data;
}

const char* ParseTimezoneOffset(const char* data, int64* offset) {
  // Accept format "HH:MM". E.g., "08:00"
  int hour;
  if ((data = ParseInt(data, 2, 0, 23, &hour)) == nullptr) {
    return nullptr;
  }
  if (*data++ != ':') {
    return nullptr;
  }
  int minute;
  if ((data = ParseInt(data, 2, 0, 59, &minute)) == nullptr) {
    return nullptr;
  }
  *offset = (hour * 60 + minute) * 60;
  return data;
}
}  // namespace

bool SecondsToDateTime(int64 seconds, DateTime* time) {
  if (seconds < kMinTime || seconds > kMaxTime) {
    return false;
  }
  // It's easier to calcuate the DateTime starting from 0001-01-01T00:00:00
  seconds = seconds + kSecondsFromEraToEpoch;
  int year = 1;
  if (seconds >= kSecondsPer400Years) {
    int count_400years = seconds / kSecondsPer400Years;
    year += 400 * count_400years;
    seconds %= kSecondsPer400Years;
  }
  while (seconds >= SecondsPer100Years(year)) {
    seconds -= SecondsPer100Years(year);
    year += 100;
  }
  while (seconds >= SecondsPer4Years(year)) {
    seconds -= SecondsPer4Years(year);
    year += 4;
  }
  while (seconds >= SecondsPerYear(year)) {
    seconds -= SecondsPerYear(year);
    year += 1;
  }
  bool leap = IsLeapYear(year);
  int month = 1;
  while (seconds >= SecondsPerMonth(month, leap)) {
    seconds -= SecondsPerMonth(month, leap);
    ++month;
  }
  int day = 1 + seconds / kSecondsPerDay;
  seconds %= kSecondsPerDay;
  int hour = seconds / kSecondsPerHour;
  seconds %= kSecondsPerHour;
  int minute = seconds / kSecondsPerMinute;
  seconds %= kSecondsPerMinute;
  time->year = year;
  time->month = month;
  time->day = day;
  time->hour = hour;
  time->minute = minute;
  time->second = static_cast<int>(seconds);
  return true;
}

bool DateTimeToSeconds(const DateTime& time, int64* seconds) {
  if (!ValidateDateTime(time)) {
    return false;
  }
  *seconds = SecondsSinceCommonEra(time) - kSecondsFromEraToEpoch;
  return true;
}

void GetCurrentTime(int64* seconds, int32* nanos) {
  // TODO(xiaofeng): Improve the accuracy of this implementation (or just
  // remove this method from protobuf).
  *seconds = time(nullptr);
  *nanos = 0;
}

string FormatTime(int64 seconds, int32 nanos) {
  DateTime time;
  if (nanos < 0 || nanos > 999999999 || !SecondsToDateTime(seconds, &time)) {
    return "InvalidTime";
  }
  string result = StringPrintf("%04d-%02d-%02dT%02d:%02d:%02d",
                               time.year, time.month, time.day,
                               time.hour, time.minute, time.second);
  if (nanos != 0) {
    result += "." + FormatNanos(nanos);
  }
  return result + "Z";
}

bool ParseTime(const string& value, int64* seconds, int32* nanos) {
  DateTime time;
  const char* data = value.c_str();
  // We only accept:
  //   Z-normalized: 2015-05-20T13:29:35.120Z
  //   With UTC offset: 2015-05-20T13:29:35.120-08:00

  // Parse year
  if ((data = ParseInt(data, 4, 1, 9999, &time.year)) == nullptr) {
    return false;
  }
  // Expect '-'
  if (*data++ != '-') return false;
  // Parse month
  if ((data = ParseInt(data, 2, 1, 12, &time.month)) == nullptr) {
    return false;
  }
  // Expect '-'
  if (*data++ != '-') return false;
  // Parse day
  if ((data = ParseInt(data, 2, 1, 31, &time.day)) == nullptr) {
    return false;
  }
  // Expect 'T'
  if (*data++ != 'T') return false;
  // Parse hour
  if ((data = ParseInt(data, 2, 0, 23, &time.hour)) == nullptr) {
    return false;
  }
  // Expect ':'
  if (*data++ != ':') return false;
  // Parse minute
  if ((data = ParseInt(data, 2, 0, 59, &time.minute)) == nullptr) {
    return false;
  }
  // Expect ':'
  if (*data++ != ':') return false;
  // Parse second
  if ((data = ParseInt(data, 2, 0, 59, &time.second)) == nullptr) {
    return false;
  }
  if (!DateTimeToSeconds(time, seconds)) {
    return false;
  }
  // Parse nanoseconds.
  if (*data == '.') {
    ++data;
    // Parse nanoseconds.
    if ((data = ParseNanos(data, nanos)) == nullptr) {
      return false;
    }
  } else {
    *nanos = 0;
  }
  // Parse UTC offsets.
  if (*data == 'Z') {
    ++data;
  } else if (*data == '+') {
    ++data;
    int64 offset;
    if ((data = ParseTimezoneOffset(data, &offset)) == nullptr) {
      return false;
    }
    *seconds -= offset;
  } else if (*data == '-') {
    ++data;
    int64 offset;
    if ((data = ParseTimezoneOffset(data, &offset)) == nullptr) {
      return false;
    }
    *seconds += offset;
  } else {
    return false;
  }
  // Done with parsing.
  return *data == 0;
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google
