// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/conversions.h"
#include "src/counters.h"
#include "src/dateparser-inl.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 20.3 Date Objects

namespace {

// ES6 section 20.3.1.1 Time Values and Time Range
const double kMinYear = -1000000.0;
const double kMaxYear = -kMinYear;
const double kMinMonth = -10000000.0;
const double kMaxMonth = -kMinMonth;

// 20.3.1.2 Day Number and Time within Day
const double kMsPerDay = 86400000.0;

// ES6 section 20.3.1.11 Hours, Minutes, Second, and Milliseconds
const double kMsPerSecond = 1000.0;
const double kMsPerMinute = 60000.0;
const double kMsPerHour = 3600000.0;

// ES6 section 20.3.1.14 MakeDate (day, time)
double MakeDate(double day, double time) {
  if (std::isfinite(day) && std::isfinite(time)) {
    return time + day * kMsPerDay;
  }
  return std::numeric_limits<double>::quiet_NaN();
}

// ES6 section 20.3.1.13 MakeDay (year, month, date)
double MakeDay(double year, double month, double date) {
  if ((kMinYear <= year && year <= kMaxYear) &&
      (kMinMonth <= month && month <= kMaxMonth) && std::isfinite(date)) {
    int y = FastD2I(year);
    int m = FastD2I(month);
    y += m / 12;
    m %= 12;
    if (m < 0) {
      m += 12;
      y -= 1;
    }
    DCHECK_LE(0, m);
    DCHECK_LT(m, 12);

    // kYearDelta is an arbitrary number such that:
    // a) kYearDelta = -1 (mod 400)
    // b) year + kYearDelta > 0 for years in the range defined by
    //    ECMA 262 - 15.9.1.1, i.e. upto 100,000,000 days on either side of
    //    Jan 1 1970. This is required so that we don't run into integer
    //    division of negative numbers.
    // c) there shouldn't be an overflow for 32-bit integers in the following
    //    operations.
    static const int kYearDelta = 399999;
    static const int kBaseDay =
        365 * (1970 + kYearDelta) + (1970 + kYearDelta) / 4 -
        (1970 + kYearDelta) / 100 + (1970 + kYearDelta) / 400;
    int day_from_year = 365 * (y + kYearDelta) + (y + kYearDelta) / 4 -
                        (y + kYearDelta) / 100 + (y + kYearDelta) / 400 -
                        kBaseDay;
    if ((y % 4 != 0) || (y % 100 == 0 && y % 400 != 0)) {
      static const int kDayFromMonth[] = {0,   31,  59,  90,  120, 151,
                                          181, 212, 243, 273, 304, 334};
      day_from_year += kDayFromMonth[m];
    } else {
      static const int kDayFromMonth[] = {0,   31,  60,  91,  121, 152,
                                          182, 213, 244, 274, 305, 335};
      day_from_year += kDayFromMonth[m];
    }
    return static_cast<double>(day_from_year - 1) + date;
  }
  return std::numeric_limits<double>::quiet_NaN();
}

// ES6 section 20.3.1.12 MakeTime (hour, min, sec, ms)
double MakeTime(double hour, double min, double sec, double ms) {
  if (std::isfinite(hour) && std::isfinite(min) && std::isfinite(sec) &&
      std::isfinite(ms)) {
    double const h = DoubleToInteger(hour);
    double const m = DoubleToInteger(min);
    double const s = DoubleToInteger(sec);
    double const milli = DoubleToInteger(ms);
    return h * kMsPerHour + m * kMsPerMinute + s * kMsPerSecond + milli;
  }
  return std::numeric_limits<double>::quiet_NaN();
}

// ES6 section 20.3.1.15 TimeClip (time)
double TimeClip(double time) {
  if (-DateCache::kMaxTimeInMs <= time && time <= DateCache::kMaxTimeInMs) {
    return DoubleToInteger(time) + 0.0;
  }
  return std::numeric_limits<double>::quiet_NaN();
}

const char* kShortWeekDays[] = {"Sun", "Mon", "Tue", "Wed",
                                "Thu", "Fri", "Sat"};
const char* kShortMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// ES6 section 20.3.1.16 Date Time String Format
double ParseDateTimeString(Handle<String> str) {
  Isolate* const isolate = str->GetIsolate();
  str = String::Flatten(str);
  // TODO(bmeurer): Change DateParser to not use the FixedArray.
  Handle<FixedArray> tmp =
      isolate->factory()->NewFixedArray(DateParser::OUTPUT_SIZE);
  DisallowHeapAllocation no_gc;
  String::FlatContent str_content = str->GetFlatContent();
  bool result;
  if (str_content.IsOneByte()) {
    result = DateParser::Parse(isolate, str_content.ToOneByteVector(), *tmp);
  } else {
    result = DateParser::Parse(isolate, str_content.ToUC16Vector(), *tmp);
  }
  if (!result) return std::numeric_limits<double>::quiet_NaN();
  double const day = MakeDay(tmp->get(0)->Number(), tmp->get(1)->Number(),
                             tmp->get(2)->Number());
  double const time = MakeTime(tmp->get(3)->Number(), tmp->get(4)->Number(),
                               tmp->get(5)->Number(), tmp->get(6)->Number());
  double date = MakeDate(day, time);
  if (tmp->get(7)->IsNull(isolate)) {
    if (!std::isnan(date)) {
      date = isolate->date_cache()->ToUTC(static_cast<int64_t>(date));
    }
  } else {
    date -= tmp->get(7)->Number() * 1000.0;
  }
  return date;
}

enum ToDateStringMode { kDateOnly, kTimeOnly, kDateAndTime };

// ES6 section 20.3.4.41.1 ToDateString(tv)
void ToDateString(double time_val, Vector<char> str, DateCache* date_cache,
                  ToDateStringMode mode = kDateAndTime) {
  if (std::isnan(time_val)) {
    SNPrintF(str, "Invalid Date");
    return;
  }
  int64_t time_ms = static_cast<int64_t>(time_val);
  int64_t local_time_ms = date_cache->ToLocal(time_ms);
  int year, month, day, weekday, hour, min, sec, ms;
  date_cache->BreakDownTime(local_time_ms, &year, &month, &day, &weekday, &hour,
                            &min, &sec, &ms);
  int timezone_offset = -date_cache->TimezoneOffset(time_ms);
  int timezone_hour = std::abs(timezone_offset) / 60;
  int timezone_min = std::abs(timezone_offset) % 60;
  const char* local_timezone = date_cache->LocalTimezone(time_ms);
  switch (mode) {
    case kDateOnly:
      SNPrintF(str, "%s %s %02d %04d", kShortWeekDays[weekday],
               kShortMonths[month], day, year);
      return;
    case kTimeOnly:
      SNPrintF(str, "%02d:%02d:%02d GMT%c%02d%02d (%s)", hour, min, sec,
               (timezone_offset < 0) ? '-' : '+', timezone_hour, timezone_min,
               local_timezone);
      return;
    case kDateAndTime:
      SNPrintF(str, "%s %s %02d %04d %02d:%02d:%02d GMT%c%02d%02d (%s)",
               kShortWeekDays[weekday], kShortMonths[month], day, year, hour,
               min, sec, (timezone_offset < 0) ? '-' : '+', timezone_hour,
               timezone_min, local_timezone);
      return;
  }
  UNREACHABLE();
}

Object* SetLocalDateValue(Handle<JSDate> date, double time_val) {
  if (time_val >= -DateCache::kMaxTimeBeforeUTCInMs &&
      time_val <= DateCache::kMaxTimeBeforeUTCInMs) {
    Isolate* const isolate = date->GetIsolate();
    time_val = isolate->date_cache()->ToUTC(static_cast<int64_t>(time_val));
  } else {
    time_val = std::numeric_limits<double>::quiet_NaN();
  }
  return *JSDate::SetValue(date, TimeClip(time_val));
}

}  // namespace

// ES6 section 20.3.2 The Date Constructor for the [[Call]] case.
BUILTIN(DateConstructor) {
  HandleScope scope(isolate);
  double const time_val = JSDate::CurrentTimeValue(isolate);
  char buffer[128];
  ToDateString(time_val, ArrayVector(buffer), isolate->date_cache());
  RETURN_RESULT_OR_FAILURE(
      isolate, isolate->factory()->NewStringFromUtf8(CStrVector(buffer)));
}

// ES6 section 20.3.2 The Date Constructor for the [[Construct]] case.
BUILTIN(DateConstructor_ConstructStub) {
  HandleScope scope(isolate);
  int const argc = args.length() - 1;
  Handle<JSFunction> target = args.target();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());
  double time_val;
  if (argc == 0) {
    time_val = JSDate::CurrentTimeValue(isolate);
  } else if (argc == 1) {
    Handle<Object> value = args.at(1);
    if (value->IsJSDate()) {
      time_val = Handle<JSDate>::cast(value)->value()->Number();
    } else {
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value,
                                         Object::ToPrimitive(value));
      if (value->IsString()) {
        time_val = ParseDateTimeString(Handle<String>::cast(value));
      } else {
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value,
                                           Object::ToNumber(value));
        time_val = value->Number();
      }
    }
  } else {
    Handle<Object> year_object;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, year_object,
                                       Object::ToNumber(args.at(1)));
    Handle<Object> month_object;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, month_object,
                                       Object::ToNumber(args.at(2)));
    double year = year_object->Number();
    double month = month_object->Number();
    double date = 1.0, hours = 0.0, minutes = 0.0, seconds = 0.0, ms = 0.0;
    if (argc >= 3) {
      Handle<Object> date_object;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, date_object,
                                         Object::ToNumber(args.at(3)));
      date = date_object->Number();
      if (argc >= 4) {
        Handle<Object> hours_object;
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, hours_object,
                                           Object::ToNumber(args.at(4)));
        hours = hours_object->Number();
        if (argc >= 5) {
          Handle<Object> minutes_object;
          ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, minutes_object,
                                             Object::ToNumber(args.at(5)));
          minutes = minutes_object->Number();
          if (argc >= 6) {
            Handle<Object> seconds_object;
            ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, seconds_object,
                                               Object::ToNumber(args.at(6)));
            seconds = seconds_object->Number();
            if (argc >= 7) {
              Handle<Object> ms_object;
              ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, ms_object,
                                                 Object::ToNumber(args.at(7)));
              ms = ms_object->Number();
            }
          }
        }
      }
    }
    if (!std::isnan(year)) {
      double const y = DoubleToInteger(year);
      if (0.0 <= y && y <= 99) year = 1900 + y;
    }
    double const day = MakeDay(year, month, date);
    double const time = MakeTime(hours, minutes, seconds, ms);
    time_val = MakeDate(day, time);
    if (time_val >= -DateCache::kMaxTimeBeforeUTCInMs &&
        time_val <= DateCache::kMaxTimeBeforeUTCInMs) {
      time_val = isolate->date_cache()->ToUTC(static_cast<int64_t>(time_val));
    } else {
      time_val = std::numeric_limits<double>::quiet_NaN();
    }
  }
  RETURN_RESULT_OR_FAILURE(isolate, JSDate::New(target, new_target, time_val));
}

// ES6 section 20.3.3.1 Date.now ( )
BUILTIN(DateNow) {
  HandleScope scope(isolate);
  return *isolate->factory()->NewNumber(JSDate::CurrentTimeValue(isolate));
}

// ES6 section 20.3.3.2 Date.parse ( string )
BUILTIN(DateParse) {
  HandleScope scope(isolate);
  Handle<String> string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, string,
      Object::ToString(isolate, args.atOrUndefined(isolate, 1)));
  return *isolate->factory()->NewNumber(ParseDateTimeString(string));
}

// ES6 section 20.3.3.4 Date.UTC (year,month,date,hours,minutes,seconds,ms)
BUILTIN(DateUTC) {
  HandleScope scope(isolate);
  int const argc = args.length() - 1;
  double year = std::numeric_limits<double>::quiet_NaN();
  double month = 0.0, date = 1.0, hours = 0.0, minutes = 0.0, seconds = 0.0,
         ms = 0.0;
  if (argc >= 1) {
    Handle<Object> year_object;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, year_object,
                                       Object::ToNumber(args.at(1)));
    year = year_object->Number();
    if (argc >= 2) {
      Handle<Object> month_object;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, month_object,
                                         Object::ToNumber(args.at(2)));
      month = month_object->Number();
      if (argc >= 3) {
        Handle<Object> date_object;
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, date_object,
                                           Object::ToNumber(args.at(3)));
        date = date_object->Number();
        if (argc >= 4) {
          Handle<Object> hours_object;
          ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, hours_object,
                                             Object::ToNumber(args.at(4)));
          hours = hours_object->Number();
          if (argc >= 5) {
            Handle<Object> minutes_object;
            ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, minutes_object,
                                               Object::ToNumber(args.at(5)));
            minutes = minutes_object->Number();
            if (argc >= 6) {
              Handle<Object> seconds_object;
              ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, seconds_object,
                                                 Object::ToNumber(args.at(6)));
              seconds = seconds_object->Number();
              if (argc >= 7) {
                Handle<Object> ms_object;
                ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
                    isolate, ms_object, Object::ToNumber(args.at(7)));
                ms = ms_object->Number();
              }
            }
          }
        }
      }
    }
  }
  if (!std::isnan(year)) {
    double const y = DoubleToInteger(year);
    if (0.0 <= y && y <= 99) year = 1900 + y;
  }
  double const day = MakeDay(year, month, date);
  double const time = MakeTime(hours, minutes, seconds, ms);
  return *isolate->factory()->NewNumber(TimeClip(MakeDate(day, time)));
}

// ES6 section 20.3.4.20 Date.prototype.setDate ( date )
BUILTIN(DatePrototypeSetDate) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setDate");
  Handle<Object> value = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value, Object::ToNumber(value));
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int64_t local_time_ms = isolate->date_cache()->ToLocal(time_ms);
    int const days = isolate->date_cache()->DaysFromTime(local_time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(local_time_ms, days);
    int year, month, day;
    isolate->date_cache()->YearMonthDayFromDays(days, &year, &month, &day);
    time_val = MakeDate(MakeDay(year, month, value->Number()), time_within_day);
  }
  return SetLocalDateValue(date, time_val);
}

// ES6 section 20.3.4.21 Date.prototype.setFullYear (year, month, date)
BUILTIN(DatePrototypeSetFullYear) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setFullYear");
  int const argc = args.length() - 1;
  Handle<Object> year = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, year, Object::ToNumber(year));
  double y = year->Number(), m = 0.0, dt = 1.0;
  int time_within_day = 0;
  if (!std::isnan(date->value()->Number())) {
    int64_t const time_ms = static_cast<int64_t>(date->value()->Number());
    int64_t local_time_ms = isolate->date_cache()->ToLocal(time_ms);
    int const days = isolate->date_cache()->DaysFromTime(local_time_ms);
    time_within_day = isolate->date_cache()->TimeInDay(local_time_ms, days);
    int year, month, day;
    isolate->date_cache()->YearMonthDayFromDays(days, &year, &month, &day);
    m = month;
    dt = day;
  }
  if (argc >= 2) {
    Handle<Object> month = args.at(2);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, month, Object::ToNumber(month));
    m = month->Number();
    if (argc >= 3) {
      Handle<Object> date = args.at(3);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, date, Object::ToNumber(date));
      dt = date->Number();
    }
  }
  double time_val = MakeDate(MakeDay(y, m, dt), time_within_day);
  return SetLocalDateValue(date, time_val);
}

// ES6 section 20.3.4.22 Date.prototype.setHours(hour, min, sec, ms)
BUILTIN(DatePrototypeSetHours) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setHours");
  int const argc = args.length() - 1;
  Handle<Object> hour = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, hour, Object::ToNumber(hour));
  double h = hour->Number();
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int64_t local_time_ms = isolate->date_cache()->ToLocal(time_ms);
    int day = isolate->date_cache()->DaysFromTime(local_time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(local_time_ms, day);
    double m = (time_within_day / (60 * 1000)) % 60;
    double s = (time_within_day / 1000) % 60;
    double milli = time_within_day % 1000;
    if (argc >= 2) {
      Handle<Object> min = args.at(2);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, min, Object::ToNumber(min));
      m = min->Number();
      if (argc >= 3) {
        Handle<Object> sec = args.at(3);
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, sec, Object::ToNumber(sec));
        s = sec->Number();
        if (argc >= 4) {
          Handle<Object> ms = args.at(4);
          ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, ms, Object::ToNumber(ms));
          milli = ms->Number();
        }
      }
    }
    time_val = MakeDate(day, MakeTime(h, m, s, milli));
  }
  return SetLocalDateValue(date, time_val);
}

// ES6 section 20.3.4.23 Date.prototype.setMilliseconds(ms)
BUILTIN(DatePrototypeSetMilliseconds) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setMilliseconds");
  Handle<Object> ms = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, ms, Object::ToNumber(ms));
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int64_t local_time_ms = isolate->date_cache()->ToLocal(time_ms);
    int day = isolate->date_cache()->DaysFromTime(local_time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(local_time_ms, day);
    int h = time_within_day / (60 * 60 * 1000);
    int m = (time_within_day / (60 * 1000)) % 60;
    int s = (time_within_day / 1000) % 60;
    time_val = MakeDate(day, MakeTime(h, m, s, ms->Number()));
  }
  return SetLocalDateValue(date, time_val);
}

// ES6 section 20.3.4.24 Date.prototype.setMinutes ( min, sec, ms )
BUILTIN(DatePrototypeSetMinutes) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setMinutes");
  int const argc = args.length() - 1;
  Handle<Object> min = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, min, Object::ToNumber(min));
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int64_t local_time_ms = isolate->date_cache()->ToLocal(time_ms);
    int day = isolate->date_cache()->DaysFromTime(local_time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(local_time_ms, day);
    int h = time_within_day / (60 * 60 * 1000);
    double m = min->Number();
    double s = (time_within_day / 1000) % 60;
    double milli = time_within_day % 1000;
    if (argc >= 2) {
      Handle<Object> sec = args.at(2);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, sec, Object::ToNumber(sec));
      s = sec->Number();
      if (argc >= 3) {
        Handle<Object> ms = args.at(3);
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, ms, Object::ToNumber(ms));
        milli = ms->Number();
      }
    }
    time_val = MakeDate(day, MakeTime(h, m, s, milli));
  }
  return SetLocalDateValue(date, time_val);
}

// ES6 section 20.3.4.25 Date.prototype.setMonth ( month, date )
BUILTIN(DatePrototypeSetMonth) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setMonth");
  int const argc = args.length() - 1;
  Handle<Object> month = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, month, Object::ToNumber(month));
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int64_t local_time_ms = isolate->date_cache()->ToLocal(time_ms);
    int days = isolate->date_cache()->DaysFromTime(local_time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(local_time_ms, days);
    int year, unused, day;
    isolate->date_cache()->YearMonthDayFromDays(days, &year, &unused, &day);
    double m = month->Number();
    double dt = day;
    if (argc >= 2) {
      Handle<Object> date = args.at(2);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, date, Object::ToNumber(date));
      dt = date->Number();
    }
    time_val = MakeDate(MakeDay(year, m, dt), time_within_day);
  }
  return SetLocalDateValue(date, time_val);
}

// ES6 section 20.3.4.26 Date.prototype.setSeconds ( sec, ms )
BUILTIN(DatePrototypeSetSeconds) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setSeconds");
  int const argc = args.length() - 1;
  Handle<Object> sec = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, sec, Object::ToNumber(sec));
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int64_t local_time_ms = isolate->date_cache()->ToLocal(time_ms);
    int day = isolate->date_cache()->DaysFromTime(local_time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(local_time_ms, day);
    int h = time_within_day / (60 * 60 * 1000);
    double m = (time_within_day / (60 * 1000)) % 60;
    double s = sec->Number();
    double milli = time_within_day % 1000;
    if (argc >= 2) {
      Handle<Object> ms = args.at(2);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, ms, Object::ToNumber(ms));
      milli = ms->Number();
    }
    time_val = MakeDate(day, MakeTime(h, m, s, milli));
  }
  return SetLocalDateValue(date, time_val);
}

// ES6 section 20.3.4.27 Date.prototype.setTime ( time )
BUILTIN(DatePrototypeSetTime) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setTime");
  Handle<Object> value = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value, Object::ToNumber(value));
  return *JSDate::SetValue(date, TimeClip(value->Number()));
}

// ES6 section 20.3.4.28 Date.prototype.setUTCDate ( date )
BUILTIN(DatePrototypeSetUTCDate) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setUTCDate");
  Handle<Object> value = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value, Object::ToNumber(value));
  if (std::isnan(date->value()->Number())) return date->value();
  int64_t const time_ms = static_cast<int64_t>(date->value()->Number());
  int const days = isolate->date_cache()->DaysFromTime(time_ms);
  int const time_within_day = isolate->date_cache()->TimeInDay(time_ms, days);
  int year, month, day;
  isolate->date_cache()->YearMonthDayFromDays(days, &year, &month, &day);
  double const time_val =
      MakeDate(MakeDay(year, month, value->Number()), time_within_day);
  return *JSDate::SetValue(date, TimeClip(time_val));
}

// ES6 section 20.3.4.29 Date.prototype.setUTCFullYear (year, month, date)
BUILTIN(DatePrototypeSetUTCFullYear) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setUTCFullYear");
  int const argc = args.length() - 1;
  Handle<Object> year = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, year, Object::ToNumber(year));
  double y = year->Number(), m = 0.0, dt = 1.0;
  int time_within_day = 0;
  if (!std::isnan(date->value()->Number())) {
    int64_t const time_ms = static_cast<int64_t>(date->value()->Number());
    int const days = isolate->date_cache()->DaysFromTime(time_ms);
    time_within_day = isolate->date_cache()->TimeInDay(time_ms, days);
    int year, month, day;
    isolate->date_cache()->YearMonthDayFromDays(days, &year, &month, &day);
    m = month;
    dt = day;
  }
  if (argc >= 2) {
    Handle<Object> month = args.at(2);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, month, Object::ToNumber(month));
    m = month->Number();
    if (argc >= 3) {
      Handle<Object> date = args.at(3);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, date, Object::ToNumber(date));
      dt = date->Number();
    }
  }
  double const time_val = MakeDate(MakeDay(y, m, dt), time_within_day);
  return *JSDate::SetValue(date, TimeClip(time_val));
}

// ES6 section 20.3.4.30 Date.prototype.setUTCHours(hour, min, sec, ms)
BUILTIN(DatePrototypeSetUTCHours) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setUTCHours");
  int const argc = args.length() - 1;
  Handle<Object> hour = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, hour, Object::ToNumber(hour));
  double h = hour->Number();
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int day = isolate->date_cache()->DaysFromTime(time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(time_ms, day);
    double m = (time_within_day / (60 * 1000)) % 60;
    double s = (time_within_day / 1000) % 60;
    double milli = time_within_day % 1000;
    if (argc >= 2) {
      Handle<Object> min = args.at(2);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, min, Object::ToNumber(min));
      m = min->Number();
      if (argc >= 3) {
        Handle<Object> sec = args.at(3);
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, sec, Object::ToNumber(sec));
        s = sec->Number();
        if (argc >= 4) {
          Handle<Object> ms = args.at(4);
          ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, ms, Object::ToNumber(ms));
          milli = ms->Number();
        }
      }
    }
    time_val = MakeDate(day, MakeTime(h, m, s, milli));
  }
  return *JSDate::SetValue(date, TimeClip(time_val));
}

// ES6 section 20.3.4.31 Date.prototype.setUTCMilliseconds(ms)
BUILTIN(DatePrototypeSetUTCMilliseconds) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setUTCMilliseconds");
  Handle<Object> ms = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, ms, Object::ToNumber(ms));
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int day = isolate->date_cache()->DaysFromTime(time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(time_ms, day);
    int h = time_within_day / (60 * 60 * 1000);
    int m = (time_within_day / (60 * 1000)) % 60;
    int s = (time_within_day / 1000) % 60;
    time_val = MakeDate(day, MakeTime(h, m, s, ms->Number()));
  }
  return *JSDate::SetValue(date, TimeClip(time_val));
}

// ES6 section 20.3.4.32 Date.prototype.setUTCMinutes ( min, sec, ms )
BUILTIN(DatePrototypeSetUTCMinutes) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setUTCMinutes");
  int const argc = args.length() - 1;
  Handle<Object> min = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, min, Object::ToNumber(min));
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int day = isolate->date_cache()->DaysFromTime(time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(time_ms, day);
    int h = time_within_day / (60 * 60 * 1000);
    double m = min->Number();
    double s = (time_within_day / 1000) % 60;
    double milli = time_within_day % 1000;
    if (argc >= 2) {
      Handle<Object> sec = args.at(2);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, sec, Object::ToNumber(sec));
      s = sec->Number();
      if (argc >= 3) {
        Handle<Object> ms = args.at(3);
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, ms, Object::ToNumber(ms));
        milli = ms->Number();
      }
    }
    time_val = MakeDate(day, MakeTime(h, m, s, milli));
  }
  return *JSDate::SetValue(date, TimeClip(time_val));
}

// ES6 section 20.3.4.31 Date.prototype.setUTCMonth ( month, date )
BUILTIN(DatePrototypeSetUTCMonth) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setUTCMonth");
  int const argc = args.length() - 1;
  Handle<Object> month = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, month, Object::ToNumber(month));
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int days = isolate->date_cache()->DaysFromTime(time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(time_ms, days);
    int year, unused, day;
    isolate->date_cache()->YearMonthDayFromDays(days, &year, &unused, &day);
    double m = month->Number();
    double dt = day;
    if (argc >= 2) {
      Handle<Object> date = args.at(2);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, date, Object::ToNumber(date));
      dt = date->Number();
    }
    time_val = MakeDate(MakeDay(year, m, dt), time_within_day);
  }
  return *JSDate::SetValue(date, TimeClip(time_val));
}

// ES6 section 20.3.4.34 Date.prototype.setUTCSeconds ( sec, ms )
BUILTIN(DatePrototypeSetUTCSeconds) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setUTCSeconds");
  int const argc = args.length() - 1;
  Handle<Object> sec = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, sec, Object::ToNumber(sec));
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int day = isolate->date_cache()->DaysFromTime(time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(time_ms, day);
    int h = time_within_day / (60 * 60 * 1000);
    double m = (time_within_day / (60 * 1000)) % 60;
    double s = sec->Number();
    double milli = time_within_day % 1000;
    if (argc >= 2) {
      Handle<Object> ms = args.at(2);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, ms, Object::ToNumber(ms));
      milli = ms->Number();
    }
    time_val = MakeDate(day, MakeTime(h, m, s, milli));
  }
  return *JSDate::SetValue(date, TimeClip(time_val));
}

// ES6 section 20.3.4.35 Date.prototype.toDateString ( )
BUILTIN(DatePrototypeToDateString) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.toDateString");
  char buffer[128];
  ToDateString(date->value()->Number(), ArrayVector(buffer),
               isolate->date_cache(), kDateOnly);
  RETURN_RESULT_OR_FAILURE(
      isolate, isolate->factory()->NewStringFromUtf8(CStrVector(buffer)));
}

// ES6 section 20.3.4.36 Date.prototype.toISOString ( )
BUILTIN(DatePrototypeToISOString) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.toISOString");
  double const time_val = date->value()->Number();
  if (std::isnan(time_val)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidTimeValue));
  }
  int64_t const time_ms = static_cast<int64_t>(time_val);
  int year, month, day, weekday, hour, min, sec, ms;
  isolate->date_cache()->BreakDownTime(time_ms, &year, &month, &day, &weekday,
                                       &hour, &min, &sec, &ms);
  char buffer[128];
  if (year >= 0 && year <= 9999) {
    SNPrintF(ArrayVector(buffer), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", year,
             month + 1, day, hour, min, sec, ms);
  } else if (year < 0) {
    SNPrintF(ArrayVector(buffer), "-%06d-%02d-%02dT%02d:%02d:%02d.%03dZ", -year,
             month + 1, day, hour, min, sec, ms);
  } else {
    SNPrintF(ArrayVector(buffer), "+%06d-%02d-%02dT%02d:%02d:%02d.%03dZ", year,
             month + 1, day, hour, min, sec, ms);
  }
  return *isolate->factory()->NewStringFromAsciiChecked(buffer);
}

// ES6 section 20.3.4.41 Date.prototype.toString ( )
BUILTIN(DatePrototypeToString) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.toString");
  char buffer[128];
  ToDateString(date->value()->Number(), ArrayVector(buffer),
               isolate->date_cache());
  RETURN_RESULT_OR_FAILURE(
      isolate, isolate->factory()->NewStringFromUtf8(CStrVector(buffer)));
}

// ES6 section 20.3.4.42 Date.prototype.toTimeString ( )
BUILTIN(DatePrototypeToTimeString) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.toTimeString");
  char buffer[128];
  ToDateString(date->value()->Number(), ArrayVector(buffer),
               isolate->date_cache(), kTimeOnly);
  RETURN_RESULT_OR_FAILURE(
      isolate, isolate->factory()->NewStringFromUtf8(CStrVector(buffer)));
}

// ES6 section 20.3.4.43 Date.prototype.toUTCString ( )
BUILTIN(DatePrototypeToUTCString) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.toUTCString");
  double const time_val = date->value()->Number();
  if (std::isnan(time_val)) {
    return *isolate->factory()->NewStringFromAsciiChecked("Invalid Date");
  }
  char buffer[128];
  int64_t time_ms = static_cast<int64_t>(time_val);
  int year, month, day, weekday, hour, min, sec, ms;
  isolate->date_cache()->BreakDownTime(time_ms, &year, &month, &day, &weekday,
                                       &hour, &min, &sec, &ms);
  SNPrintF(ArrayVector(buffer), "%s, %02d %s %04d %02d:%02d:%02d GMT",
           kShortWeekDays[weekday], day, kShortMonths[month], year, hour, min,
           sec);
  return *isolate->factory()->NewStringFromAsciiChecked(buffer);
}

// ES6 section B.2.4.1 Date.prototype.getYear ( )
BUILTIN(DatePrototypeGetYear) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.getYear");
  double time_val = date->value()->Number();
  if (std::isnan(time_val)) return date->value();
  int64_t time_ms = static_cast<int64_t>(time_val);
  int64_t local_time_ms = isolate->date_cache()->ToLocal(time_ms);
  int days = isolate->date_cache()->DaysFromTime(local_time_ms);
  int year, month, day;
  isolate->date_cache()->YearMonthDayFromDays(days, &year, &month, &day);
  return Smi::FromInt(year - 1900);
}

// ES6 section B.2.4.2 Date.prototype.setYear ( year )
BUILTIN(DatePrototypeSetYear) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setYear");
  Handle<Object> year = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, year, Object::ToNumber(year));
  double m = 0.0, dt = 1.0, y = year->Number();
  if (0.0 <= y && y <= 99.0) {
    y = 1900.0 + DoubleToInteger(y);
  }
  int time_within_day = 0;
  if (!std::isnan(date->value()->Number())) {
    int64_t const time_ms = static_cast<int64_t>(date->value()->Number());
    int64_t local_time_ms = isolate->date_cache()->ToLocal(time_ms);
    int const days = isolate->date_cache()->DaysFromTime(local_time_ms);
    time_within_day = isolate->date_cache()->TimeInDay(local_time_ms, days);
    int year, month, day;
    isolate->date_cache()->YearMonthDayFromDays(days, &year, &month, &day);
    m = month;
    dt = day;
  }
  double time_val = MakeDate(MakeDay(y, m, dt), time_within_day);
  return SetLocalDateValue(date, time_val);
}

// ES6 section 20.3.4.37 Date.prototype.toJSON ( key )
BUILTIN(DatePrototypeToJson) {
  HandleScope scope(isolate);
  Handle<Object> receiver = args.atOrUndefined(isolate, 0);
  Handle<JSReceiver> receiver_obj;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver_obj,
                                     Object::ToObject(isolate, receiver));
  Handle<Object> primitive;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, primitive,
      Object::ToPrimitive(receiver_obj, ToPrimitiveHint::kNumber));
  if (primitive->IsNumber() && !std::isfinite(primitive->Number())) {
    return isolate->heap()->null_value();
  } else {
    Handle<String> name =
        isolate->factory()->NewStringFromAsciiChecked("toISOString");
    Handle<Object> function;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, function,
                                       Object::GetProperty(receiver_obj, name));
    if (!function->IsCallable()) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewTypeError(MessageTemplate::kCalledNonCallable, name));
    }
    RETURN_RESULT_OR_FAILURE(
        isolate, Execution::Call(isolate, function, receiver_obj, 0, nullptr));
  }
}

}  // namespace internal
}  // namespace v8
