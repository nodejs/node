// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/js-temporal-objects.h"

#include <set>

#include "src/base/optional.h"
#include "src/common/globals.h"
#include "src/date/date.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/numbers/conversions-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/intl-objects.h"
#include "src/objects/js-date-time-format.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-objects-inl.h"
#include "src/objects/js-objects.h"
#include "src/objects/js-temporal-objects-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/managed-inl.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/objects-inl.h"
#include "src/objects/option-utils.h"
#include "src/objects/property-descriptor.h"
#include "src/strings/string-builder-inl.h"
#include "src/temporal/temporal-parser.h"
#ifdef V8_INTL_SUPPORT
#include "unicode/calendar.h"
#include "unicode/unistr.h"
#endif  // V8_INTL_SUPPORT

namespace v8 {
namespace internal {

namespace {

enum class Unit {
  kNotPresent,
  kAuto,
  kYear,
  kMonth,
  kWeek,
  kDay,
  kHour,
  kMinute,
  kSecond,
  kMillisecond,
  kMicrosecond,
  kNanosecond
};

/**
 * This header declare the Abstract Operations defined in the
 * Temporal spec with the enum and struct for them.
 */

// Struct
struct DateRecordCommon {
  int32_t year;
  int32_t month;
  int32_t day;
};

struct TimeRecordCommon {
  int32_t hour;
  int32_t minute;
  int32_t second;
  int32_t millisecond;
  int32_t microsecond;
  int32_t nanosecond;
};

struct DateTimeRecordCommon {
  DateRecordCommon date;
  TimeRecordCommon time;
};

struct DateRecord {
  DateRecordCommon date;
  Handle<String> calendar;
};

struct TimeRecord {
  TimeRecordCommon time;
  Handle<String> calendar;
};

struct DateTimeRecord {
  DateRecordCommon date;
  TimeRecordCommon time;
  Handle<String> calendar;
};

struct InstantRecord {
  DateRecordCommon date;
  TimeRecordCommon time;
  Handle<String> offset_string;
};

// #sec-temporal-time-duration-records
struct TimeDurationRecord {
  double days;
  double hours;
  double minutes;
  double seconds;
  double milliseconds;
  double microseconds;
  double nanoseconds;

  // #sec-temporal-createtimedurationrecord
  static Maybe<TimeDurationRecord> Create(Isolate* isolate, double days,
                                          double hours, double minutes,
                                          double seconds, double milliseconds,
                                          double microseconds,
                                          double nanoseconds);
};

// #sec-temporal-duration-records
// Cannot reuse DateDurationRecord here due to duplicate days.
struct DurationRecord {
  double years;
  double months;
  double weeks;
  TimeDurationRecord time_duration;
  // #sec-temporal-createdurationrecord
  static Maybe<DurationRecord> Create(Isolate* isolate, double years,
                                      double months, double weeks, double days,
                                      double hours, double minutes,
                                      double seconds, double milliseconds,
                                      double microseconds, double nanoseconds);
};

// #sec-temporal-isvalidduration
bool IsValidDuration(Isolate* isolate, const DurationRecord& dur);

// #sec-temporal-date-duration-records
struct DateDurationRecord {
  double years;
  double months;
  double weeks;
  double days;
  // #sec-temporal-createdatedurationrecord
  static Maybe<DateDurationRecord> Create(Isolate* isolate, double years,
                                          double months, double weeks,
                                          double days);
};

struct TimeZoneRecord {
  bool z;
  Handle<String> offset_string;
  Handle<String> name;
};

// Options

V8_WARN_UNUSED_RESULT Handle<String> UnitToString(Isolate* isolate, Unit unit);

// #sec-temporal-totemporaldisambiguation
enum class Disambiguation { kCompatible, kEarlier, kLater, kReject };

// #sec-temporal-totemporaloverflow
enum class ShowOverflow { kConstrain, kReject };

// ISO8601 String Parsing

// #sec-temporal-parsetemporalcalendarstring
V8_WARN_UNUSED_RESULT MaybeHandle<String> ParseTemporalCalendarString(
    Isolate* isolate, Handle<String> iso_string);

// #sec-temporal-parsetemporaldatestring
V8_WARN_UNUSED_RESULT Maybe<DateRecord> ParseTemporalDateString(
    Isolate* isolate, Handle<String> iso_string);

// #sec-temporal-parsetemporaltimestring
Maybe<TimeRecord> ParseTemporalTimeString(Isolate* isolate,
                                          Handle<String> iso_string);
// #sec-temporal-parsetemporaldurationstring
V8_WARN_UNUSED_RESULT Maybe<DurationRecord> ParseTemporalDurationString(
    Isolate* isolate, Handle<String> iso_string);

// #sec-temporal-parsetemporaltimezone
V8_WARN_UNUSED_RESULT MaybeHandle<String> ParseTemporalTimeZone(
    Isolate* isolate, Handle<String> string);

// #sec-temporal-parsetemporaltimezonestring
V8_WARN_UNUSED_RESULT Maybe<TimeZoneRecord> ParseTemporalTimeZoneString(
    Isolate* isolate, Handle<String> iso_string);

// #sec-temporal-parsetimezoneoffsetstring
V8_WARN_UNUSED_RESULT Maybe<int64_t> ParseTimeZoneOffsetString(
    Isolate* isolate, Handle<String> offset_string,
    bool throwIfNotSatisfy = true);

// #sec-temporal-parsetemporalinstant
V8_WARN_UNUSED_RESULT MaybeHandle<BigInt> ParseTemporalInstant(
    Isolate* isolate, Handle<String> iso_string);

DateRecordCommon BalanceISODate(Isolate* isolate, const DateRecordCommon& date);

// Math and Misc

V8_WARN_UNUSED_RESULT MaybeHandle<BigInt> AddInstant(
    Isolate* isolate, Handle<BigInt> epoch_nanoseconds,
    const TimeDurationRecord& addend);

// #sec-temporal-balanceduration
V8_WARN_UNUSED_RESULT Maybe<TimeDurationRecord> BalanceDuration(
    Isolate* isolate, Unit largest_unit, Handle<Object> relative_to,
    const TimeDurationRecord& duration, const char* method_name);

V8_WARN_UNUSED_RESULT Maybe<DurationRecord> DifferenceISODateTime(
    Isolate* isolate, const DateTimeRecordCommon& date_time1,
    const DateTimeRecordCommon& date_time2, Handle<JSReceiver> calendar,
    Unit largest_unit, Handle<Object> relative_to, const char* method_name);

// #sec-temporal-adddatetime
V8_WARN_UNUSED_RESULT Maybe<DateTimeRecordCommon> AddDateTime(
    Isolate* isolate, const DateTimeRecordCommon& date_time,
    Handle<JSReceiver> calendar, const DurationRecord& addend,
    Handle<Object> options);

// #sec-temporal-addzoneddatetime
V8_WARN_UNUSED_RESULT MaybeHandle<BigInt> AddZonedDateTime(
    Isolate* isolate, Handle<BigInt> eopch_nanoseconds,
    Handle<JSReceiver> time_zone, Handle<JSReceiver> calendar,
    const DurationRecord& addend, const char* method_name);

V8_WARN_UNUSED_RESULT MaybeHandle<BigInt> AddZonedDateTime(
    Isolate* isolate, Handle<BigInt> eopch_nanoseconds,
    Handle<JSReceiver> time_zone, Handle<JSReceiver> calendar,
    const DurationRecord& addend, Handle<JSReceiver> options,
    const char* method_name);

// #sec-temporal-isvalidepochnanoseconds
bool IsValidEpochNanoseconds(Isolate* isolate,
                             Handle<BigInt> epoch_nanoseconds);

struct NanosecondsToDaysResult {
  double days;
  double nanoseconds;
  int64_t day_length;
};

// #sec-temporal-nanosecondstodays
V8_WARN_UNUSED_RESULT Maybe<NanosecondsToDaysResult> NanosecondsToDays(
    Isolate* isolate, Handle<BigInt> nanoseconds,
    Handle<Object> relative_to_obj, const char* method_name);

V8_WARN_UNUSED_RESULT Maybe<NanosecondsToDaysResult> NanosecondsToDays(
    Isolate* isolate, double nanoseconds, Handle<Object> relative_to_obj,
    const char* method_name);

// #sec-temporal-interpretisodatetimeoffset
enum class OffsetBehaviour { kOption, kExact, kWall };

V8_WARN_UNUSED_RESULT
Handle<BigInt> GetEpochFromISOParts(Isolate* isolate,
                                    const DateTimeRecordCommon& date_time);

int32_t DurationSign(Isolate* isolaet, const DurationRecord& dur);

// #sec-temporal-isodaysinmonth
int32_t ISODaysInMonth(Isolate* isolate, int32_t year, int32_t month);

// #sec-temporal-isodaysinyear
int32_t ISODaysInYear(Isolate* isolate, int32_t year);

bool IsValidTime(Isolate* isolate, const TimeRecordCommon& time);

// #sec-temporal-isvalidisodate
bool IsValidISODate(Isolate* isolate, const DateRecordCommon& date);

// #sec-temporal-compareisodate
int32_t CompareISODate(Isolate* isolate, const DateRecordCommon& date1,
                       const DateRecordCommon& date2);

// #sec-temporal-balanceisoyearmonth
void BalanceISOYearMonth(Isolate* isolate, int32_t* year, int32_t* month);

// #sec-temporal-balancetime
V8_WARN_UNUSED_RESULT DateTimeRecordCommon
BalanceTime(const TimeRecordCommon& time);

// #sec-temporal-differencetime
V8_WARN_UNUSED_RESULT Maybe<TimeDurationRecord> DifferenceTime(
    Isolate* isolate, const TimeRecordCommon& time1,
    const TimeRecordCommon& time2);

// #sec-temporal-addtime
V8_WARN_UNUSED_RESULT DateTimeRecordCommon
AddTime(Isolate* isolate, const TimeRecordCommon& time,
        const TimeDurationRecord& addend);

// #sec-temporal-totaldurationnanoseconds
double TotalDurationNanoseconds(Isolate* isolate,
                                const TimeDurationRecord& duration,
                                double offset_shift);

// #sec-temporal-totemporaltimerecord
Maybe<TimeRecord> ToTemporalTimeRecord(Isolate* isolate,
                                       Handle<JSReceiver> temporal_time_like,
                                       const char* method_name);
// Calendar Operations

// #sec-temporal-calendardateadd
V8_WARN_UNUSED_RESULT MaybeHandle<JSTemporalPlainDate> CalendarDateAdd(
    Isolate* isolate, Handle<JSReceiver> calendar, Handle<Object> date,
    Handle<Object> durations, Handle<Object> options, Handle<Object> date_add);

// #sec-temporal-calendardateuntil
V8_WARN_UNUSED_RESULT MaybeHandle<JSTemporalDuration> CalendarDateUntil(
    Isolate* isolate, Handle<JSReceiver> calendar, Handle<Object> one,
    Handle<Object> two, Handle<Object> options, Handle<Object> date_until);

// #sec-temporal-calendarfields
MaybeHandle<FixedArray> CalendarFields(Isolate* isolate,
                                       Handle<JSReceiver> calendar,
                                       Handle<FixedArray> field_names);

// #sec-temporal-getoffsetnanosecondsfor
V8_WARN_UNUSED_RESULT Maybe<int64_t> GetOffsetNanosecondsFor(
    Isolate* isolate, Handle<JSReceiver> time_zone, Handle<Object> instant,
    const char* method_name);

// #sec-temporal-totemporalcalendarwithisodefault
MaybeHandle<JSReceiver> ToTemporalCalendarWithISODefault(
    Isolate* isolate, Handle<Object> temporal_calendar_like,
    const char* method_name);

// #sec-temporal-isbuiltincalendar
bool IsBuiltinCalendar(Isolate* isolate, Handle<String> id);

// Internal Helper Function
int32_t CalendarIndex(Isolate* isolate, Handle<String> id);

// #sec-isvalidtimezonename
bool IsValidTimeZoneName(Isolate* isolate, Handle<String> time_zone);

// #sec-canonicalizetimezonename
V8_WARN_UNUSED_RESULT MaybeHandle<String> CanonicalizeTimeZoneName(
    Isolate* isolate, Handle<String> identifier);

// #sec-temporal-tointegerthrowoninfinity
MaybeHandle<Object> ToIntegerThrowOnInfinity(Isolate* isolate,
                                             Handle<Object> argument);

// #sec-temporal-topositiveinteger
MaybeHandle<Object> ToPositiveInteger(Isolate* isolate,
                                      Handle<Object> argument);

inline double modulo(double a, int32_t b) { return a - std::floor(a / b) * b; }

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define AT __FILE__ ":" TOSTRING(__LINE__)

#ifdef DEBUG
#define TEMPORAL_DEBUG_INFO AT
#define TEMPORAL_ENTER_FUNC()
// #define TEMPORAL_ENTER_FUNC()  do { PrintF("Start: %s\n", __func__); } while
// (false)
#else
// #define TEMPORAL_DEBUG_INFO ""
#define TEMPORAL_DEBUG_INFO AT
#define TEMPORAL_ENTER_FUNC()
// #define TEMPORAL_ENTER_FUNC()  do { PrintF("Start: %s\n", __func__); } while
// (false)
#endif  // DEBUG

#define NEW_TEMPORAL_INVALID_ARG_TYPE_ERROR()       \
  NewTypeError(                                     \
      MessageTemplate::kInvalidArgumentForTemporal, \
      isolate->factory()->NewStringFromStaticChars(TEMPORAL_DEBUG_INFO))

#define NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR()       \
  NewRangeError(                                     \
      MessageTemplate::kInvalidTimeValueForTemporal, \
      isolate->factory()->NewStringFromStaticChars(TEMPORAL_DEBUG_INFO))

// #sec-defaulttimezone
MaybeHandle<String> DefaultTimeZone(Isolate* isolate) {
  TEMPORAL_ENTER_FUNC();
  // For now, always return "UTC"
  // TODO(ftang) implement behavior specified in  #sup-defaulttimezone
  return isolate->factory()->UTC_string();
}

// #sec-temporal-isodatetimewithinlimits
bool ISODateTimeWithinLimits(Isolate* isolate,
                             const DateTimeRecordCommon& date_time) {
  TEMPORAL_ENTER_FUNC();
  /**
   * Note: It is really overkill to decide within the limit by following the
   * specified algorithm literally, which require the conversion to BigInt.
   * Take a short cut and use pre-calculated year/month/day boundary instead.
   *
   * Math:
   * (-8.64 x 10^21- 8.64 x 10^16,  8.64 x 10^21 + 8.64 x 10^16) ns
   * = (-8.64 x 9999 x 10^16,  8.64 x 9999 x 10^16) ns
   * = (-8.64 x 9999 x 10^10,  8.64 x 9999 x 10^10) millisecond
   * = (-8.64 x 9999 x 10^7,  8.64 x 9999 x 10^7) second
   * = (-86400 x 9999 x 10^3,  86400 x 9999 x 10^3) second
   * = (-9999 x 10^3,  9999 x 10^3) days => Because 60*60*24 = 86400
   * 9999000 days is about 27376 years, 4 months and 7 days.
   * Therefore 9999000 days before Jan 1 1970 is around Auguest 23, -25407 and
   * 9999000 days after Jan 1 1970 is around April 9, 29346.
   */
  if (date_time.date.year > -25407 && date_time.date.year < 29346) return true;
  if (date_time.date.year < -25407 || date_time.date.year > 29346) return false;
  if (date_time.date.year == -25407) {
    if (date_time.date.month > 8) return true;
    if (date_time.date.month < 8) return false;
    return (date_time.date.day > 23);
  } else {
    DCHECK_EQ(date_time.date.year, 29346);
    if (date_time.date.month > 4) return false;
    if (date_time.date.month < 4) return true;
    return (date_time.date.day > 23);
  }
  // 1. Assert: year, month, day, hour, minute, second, millisecond,
  // microsecond, and nanosecond are integers.
  // 2. Let ns be ! GetEpochFromISOParts(year, month, day, hour, minute,
  // second, millisecond, microsecond, nanosecond).
  // 3. If ns ≤ -8.64 × 10^21 - 8.64 × 10^16, then
  // 4. If ns ≥ 8.64 × 10^21 + 8.64 × 10^16, then
  // 5. Return true.
}

// #sec-temporal-isoyearmonthwithinlimits
bool ISOYearMonthWithinLimits(int32_t year, int32_t month) {
  TEMPORAL_ENTER_FUNC();
  // 1. Assert: year and month are integers.
  // 2. If year < −271821 or year > 275760, then
  // a. Return false.
  if (year < -271821 || year > 275760) return false;
  // 3. If year is −271821 and month < 4, then
  // a. Return false.
  if (year == -271821 && month < 4) return false;
  // 4. If year is 275760 and month > 9, then
  // a. Return false.
  if (year == 275760 && month > 9) return false;
  // 5. Return true.
  return true;
}

#define ORDINARY_CREATE_FROM_CONSTRUCTOR(obj, target, new_target, T)       \
  Handle<JSReceiver> new_target_receiver =                                 \
      Handle<JSReceiver>::cast(new_target);                                \
  Handle<Map> map;                                                         \
  ASSIGN_RETURN_ON_EXCEPTION(                                              \
      isolate, map,                                                        \
      JSFunction::GetDerivedMap(isolate, target, new_target_receiver), T); \
  Handle<T> object =                                                       \
      Handle<T>::cast(isolate->factory()->NewFastOrSlowJSObjectFromMap(map));

#define THROW_INVALID_RANGE(T) \
  THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(), T);

#define CONSTRUCTOR(name)                                                    \
  Handle<JSFunction>(                                                        \
      JSFunction::cast(                                                      \
          isolate->context().native_context().temporal_##name##_function()), \
      isolate)

// #sec-temporal-systemutcepochnanoseconds
MaybeHandle<BigInt> SystemUTCEpochNanoseconds(Isolate* isolate) {
  TEMPORAL_ENTER_FUNC();
  // 1. Let ns be the approximate current UTC date and time, in nanoseconds
  // since the epoch.
  double ms = V8::GetCurrentPlatform()->CurrentClockTimeMillis();
  // 2. Set ns to the result of clamping ns between −8.64 × 10^21 and 8.64 ×
  // 10^21.

  // 3. Return ℤ(ns).
  double ns = ms * 1000000.0;
  ns = std::floor(std::max(-8.64e21, std::min(ns, 8.64e21)));
  return BigInt::FromNumber(isolate, isolate->factory()->NewNumber(ns));
}

// #sec-temporal-createtemporalcalendar
MaybeHandle<JSTemporalCalendar> CreateTemporalCalendar(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    Handle<String> identifier) {
  TEMPORAL_ENTER_FUNC();
  // 1. Assert: ! IsBuiltinCalendar(identifier) is true.
  // 2. If newTarget is not provided, set newTarget to %Temporal.Calendar%.
  // 3. Let object be ? OrdinaryCreateFromConstructor(newTarget,
  // "%Temporal.Calendar.prototype%", « [[InitializedTemporalCalendar]],
  // [[Identifier]] »).
  int32_t index = CalendarIndex(isolate, identifier);

  ORDINARY_CREATE_FROM_CONSTRUCTOR(object, target, new_target,
                                   JSTemporalCalendar)

  object->set_flags(0);
  // 4. Set object.[[Identifier]] to identifier.
  object->set_calendar_index(index);
  // 5. Return object.
  return object;
}

MaybeHandle<JSTemporalCalendar> CreateTemporalCalendar(
    Isolate* isolate, Handle<String> identifier) {
  TEMPORAL_ENTER_FUNC();
  return CreateTemporalCalendar(isolate, CONSTRUCTOR(calendar),
                                CONSTRUCTOR(calendar), identifier);
}

// #sec-temporal-createtemporaldate
MaybeHandle<JSTemporalPlainDate> CreateTemporalDate(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    const DateRecordCommon& date, Handle<JSReceiver> calendar) {
  TEMPORAL_ENTER_FUNC();
  // 1. Assert: isoYear is an integer.
  // 2. Assert: isoMonth is an integer.
  // 3. Assert: isoDay is an integer.
  // 4. Assert: Type(calendar) is Object.
  // 5. If ! IsValidISODate(isoYear, isoMonth, isoDay) is false, throw a
  // RangeError exception.
  if (!IsValidISODate(isolate, date)) {
    THROW_INVALID_RANGE(JSTemporalPlainDate);
  }
  // 6. If ! ISODateTimeWithinLimits(isoYear, isoMonth, isoDay, 12, 0, 0, 0, 0,
  // 0) is false, throw a RangeError exception.
  if (!ISODateTimeWithinLimits(isolate, {date, {12, 0, 0, 0, 0, 0}})) {
    THROW_INVALID_RANGE(JSTemporalPlainDate);
  }
  // 7. If newTarget is not present, set it to %Temporal.PlainDate%.

  // 8. Let object be ? OrdinaryCreateFromConstructor(newTarget,
  // "%Temporal.PlainDate.prototype%", « [[InitializedTemporalDate]],
  // [[ISOYear]], [[ISOMonth]], [[ISODay]], [[Calendar]] »).
  ORDINARY_CREATE_FROM_CONSTRUCTOR(object, target, new_target,
                                   JSTemporalPlainDate)
  object->set_year_month_day(0);
  // 9. Set object.[[ISOYear]] to isoYear.
  object->set_iso_year(date.year);
  // 10. Set object.[[ISOMonth]] to isoMonth.
  object->set_iso_month(date.month);
  // 11. Set object.[[ISODay]] to isoDay.
  object->set_iso_day(date.day);
  // 12. Set object.[[Calendar]] to calendar.
  object->set_calendar(*calendar);
  // 13. Return object.
  return object;
}

MaybeHandle<JSTemporalPlainDate> CreateTemporalDate(
    Isolate* isolate, const DateRecordCommon& date,
    Handle<JSReceiver> calendar) {
  TEMPORAL_ENTER_FUNC();
  return CreateTemporalDate(isolate, CONSTRUCTOR(plain_date),
                            CONSTRUCTOR(plain_date), date, calendar);
}

// #sec-temporal-createtemporaldatetime
MaybeHandle<JSTemporalPlainDateTime> CreateTemporalDateTime(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    const DateTimeRecordCommon& date_time, Handle<JSReceiver> calendar) {
  TEMPORAL_ENTER_FUNC();
  // 1. Assert: isoYear, isoMonth, isoDay, hour, minute, second, millisecond,
  // microsecond, and nanosecond are integers.
  // 2. Assert: Type(calendar) is Object.
  // 3. If ! IsValidISODate(isoYear, isoMonth, isoDay) is false, throw a
  // RangeError exception.
  if (!IsValidISODate(isolate, date_time.date)) {
    THROW_INVALID_RANGE(JSTemporalPlainDateTime);
  }
  // 4. If ! IsValidTime(hour, minute, second, millisecond, microsecond,
  // nanosecond) is false, throw a RangeError exception.
  if (!IsValidTime(isolate, date_time.time)) {
    THROW_INVALID_RANGE(JSTemporalPlainDateTime);
  }
  // 5. If ! ISODateTimeWithinLimits(isoYear, isoMonth, isoDay, hour, minute,
  // second, millisecond, microsecond, nanosecond) is false, then
  if (!ISODateTimeWithinLimits(isolate, date_time)) {
    // a. Throw a RangeError exception.
    THROW_INVALID_RANGE(JSTemporalPlainDateTime);
  }
  // 6. If newTarget is not present, set it to %Temporal.PlainDateTime%.
  // 7. Let object be ? OrdinaryCreateFromConstructor(newTarget,
  // "%Temporal.PlainDateTime.prototype%", « [[InitializedTemporalDateTime]],
  // [[ISOYear]], [[ISOMonth]], [[ISODay]], [[ISOHour]], [[ISOMinute]],
  // [[ISOSecond]], [[ISOMillisecond]], [[ISOMicrosecond]], [[ISONanosecond]],
  // [[Calendar]] »).
  ORDINARY_CREATE_FROM_CONSTRUCTOR(object, target, new_target,
                                   JSTemporalPlainDateTime)

  object->set_year_month_day(0);
  object->set_hour_minute_second(0);
  object->set_second_parts(0);
  // 8. Set object.[[ISOYear]] to isoYear.
  object->set_iso_year(date_time.date.year);
  // 9. Set object.[[ISOMonth]] to isoMonth.
  object->set_iso_month(date_time.date.month);
  // 10. Set object.[[ISODay]] to isoDay.
  object->set_iso_day(date_time.date.day);
  // 11. Set object.[[ISOHour]] to hour.
  object->set_iso_hour(date_time.time.hour);
  // 12. Set object.[[ISOMinute]] to minute.
  object->set_iso_minute(date_time.time.minute);
  // 13. Set object.[[ISOSecond]] to second.
  object->set_iso_second(date_time.time.second);
  // 14. Set object.[[ISOMillisecond]] to millisecond.
  object->set_iso_millisecond(date_time.time.millisecond);
  // 15. Set object.[[ISOMicrosecond]] to microsecond.
  object->set_iso_microsecond(date_time.time.microsecond);
  // 16. Set object.[[ISONanosecond]] to nanosecond.
  object->set_iso_nanosecond(date_time.time.nanosecond);
  // 17. Set object.[[Calendar]] to calendar.
  object->set_calendar(*calendar);
  // 18. Return object.
  return object;
}

MaybeHandle<JSTemporalPlainDateTime> CreateTemporalDateTimeDefaultTarget(
    Isolate* isolate, const DateTimeRecordCommon& date_time,
    Handle<JSReceiver> calendar) {
  TEMPORAL_ENTER_FUNC();
  return CreateTemporalDateTime(isolate, CONSTRUCTOR(plain_date_time),
                                CONSTRUCTOR(plain_date_time), date_time,
                                calendar);
}

}  // namespace

namespace temporal {

MaybeHandle<JSTemporalPlainDateTime> CreateTemporalDateTime(
    Isolate* isolate, const DateTimeRecordCommon& date_time,
    Handle<JSReceiver> calendar) {
  return CreateTemporalDateTimeDefaultTarget(isolate, date_time, calendar);
}

}  // namespace temporal

namespace {
// #sec-temporal-createtemporaltime
MaybeHandle<JSTemporalPlainTime> CreateTemporalTime(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    const TimeRecordCommon& time) {
  TEMPORAL_ENTER_FUNC();
  // 2. If ! IsValidTime(hour, minute, second, millisecond, microsecond,
  // nanosecond) is false, throw a RangeError exception.
  if (!IsValidTime(isolate, time)) {
    THROW_INVALID_RANGE(JSTemporalPlainTime);
  }

  // 4. Let object be ? OrdinaryCreateFromConstructor(newTarget,
  // "%Temporal.PlainTime.prototype%", « [[InitializedTemporalTime]],
  // [[ISOHour]], [[ISOMinute]], [[ISOSecond]], [[ISOMillisecond]],
  // [[ISOMicrosecond]], [[ISONanosecond]], [[Calendar]] »).
  ORDINARY_CREATE_FROM_CONSTRUCTOR(object, target, new_target,
                                   JSTemporalPlainTime)
  Handle<JSTemporalCalendar> calendar;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, calendar,
                             temporal::GetISO8601Calendar(isolate),
                             JSTemporalPlainTime);
  object->set_hour_minute_second(0);
  object->set_second_parts(0);
  // 5. Set object.[[ISOHour]] to hour.
  object->set_iso_hour(time.hour);
  // 6. Set object.[[ISOMinute]] to minute.
  object->set_iso_minute(time.minute);
  // 7. Set object.[[ISOSecond]] to second.
  object->set_iso_second(time.second);
  // 8. Set object.[[ISOMillisecond]] to millisecond.
  object->set_iso_millisecond(time.millisecond);
  // 9. Set object.[[ISOMicrosecond]] to microsecond.
  object->set_iso_microsecond(time.microsecond);
  // 10. Set object.[[ISONanosecond]] to nanosecond.
  object->set_iso_nanosecond(time.nanosecond);
  // 11. Set object.[[Calendar]] to ? GetISO8601Calendar().
  object->set_calendar(*calendar);

  // 12. Return object.
  return object;
}

MaybeHandle<JSTemporalPlainTime> CreateTemporalTime(
    Isolate* isolate, const TimeRecordCommon& time) {
  TEMPORAL_ENTER_FUNC();
  return CreateTemporalTime(isolate, CONSTRUCTOR(plain_time),
                            CONSTRUCTOR(plain_time), time);
}

// #sec-temporal-createtemporalmonthday
MaybeHandle<JSTemporalPlainMonthDay> CreateTemporalMonthDay(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    int32_t iso_month, int32_t iso_day, Handle<JSReceiver> calendar,
    int32_t reference_iso_year) {
  TEMPORAL_ENTER_FUNC();
  // 1. Assert: isoMonth, isoDay, and referenceISOYear are integers.
  // 2. Assert: Type(calendar) is Object.
  // 3. If ! IsValidISODate(referenceISOYear, isoMonth, isoDay) is false, throw
  // a RangeError exception.
  if (!IsValidISODate(isolate, {reference_iso_year, iso_month, iso_day})) {
    THROW_INVALID_RANGE(JSTemporalPlainMonthDay);
  }
  // 4. If newTarget is not present, set it to %Temporal.PlainMonthDay%.
  // 5. Let object be ? OrdinaryCreateFromConstructor(newTarget,
  // "%Temporal.PlainMonthDay.prototype%", « [[InitializedTemporalMonthDay]],
  // [[ISOMonth]], [[ISODay]], [[ISOYear]], [[Calendar]] »).
  ORDINARY_CREATE_FROM_CONSTRUCTOR(object, target, new_target,
                                   JSTemporalPlainMonthDay)
  object->set_year_month_day(0);
  // 6. Set object.[[ISOMonth]] to isoMonth.
  object->set_iso_month(iso_month);
  // 7. Set object.[[ISODay]] to isoDay.
  object->set_iso_day(iso_day);
  // 8. Set object.[[Calendar]] to calendar.
  object->set_calendar(*calendar);
  // 9. Set object.[[ISOYear]] to referenceISOYear.
  object->set_iso_year(reference_iso_year);
  // 10. Return object.
  return object;
}

MaybeHandle<JSTemporalPlainMonthDay> CreateTemporalMonthDay(
    Isolate* isolate, int32_t iso_month, int32_t iso_day,
    Handle<JSReceiver> calendar, int32_t reference_iso_year) {
  TEMPORAL_ENTER_FUNC();
  return CreateTemporalMonthDay(isolate, CONSTRUCTOR(plain_month_day),
                                CONSTRUCTOR(plain_month_day), iso_month,
                                iso_day, calendar, reference_iso_year);
}

// #sec-temporal-createtemporalyearmonth
MaybeHandle<JSTemporalPlainYearMonth> CreateTemporalYearMonth(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    int32_t iso_year, int32_t iso_month, Handle<JSReceiver> calendar,
    int32_t reference_iso_day) {
  TEMPORAL_ENTER_FUNC();
  // 1. Assert: isoYear, isoMonth, and referenceISODay are integers.
  // 2. Assert: Type(calendar) is Object.
  // 3. If ! IsValidISODate(isoYear, isoMonth, referenceISODay) is false, throw
  // a RangeError exception.
  if (!IsValidISODate(isolate, {iso_year, iso_month, reference_iso_day})) {
    THROW_INVALID_RANGE(JSTemporalPlainYearMonth);
  }
  // 4. If ! ISOYearMonthWithinLimits(isoYear, isoMonth) is false, throw a
  // RangeError exception.
  if (!ISOYearMonthWithinLimits(iso_year, iso_month)) {
    THROW_INVALID_RANGE(JSTemporalPlainYearMonth);
  }
  // 5. If newTarget is not present, set it to %Temporal.PlainYearMonth%.
  // 6. Let object be ? OrdinaryCreateFromConstructor(newTarget,
  // "%Temporal.PlainYearMonth.prototype%", « [[InitializedTemporalYearMonth]],
  // [[ISOYear]], [[ISOMonth]], [[ISODay]], [[Calendar]] »).
  ORDINARY_CREATE_FROM_CONSTRUCTOR(object, target, new_target,
                                   JSTemporalPlainYearMonth)
  object->set_year_month_day(0);
  // 7. Set object.[[ISOYear]] to isoYear.
  object->set_iso_year(iso_year);
  // 8. Set object.[[ISOMonth]] to isoMonth.
  object->set_iso_month(iso_month);
  // 9. Set object.[[Calendar]] to calendar.
  object->set_calendar(*calendar);
  // 10. Set object.[[ISODay]] to referenceISODay.
  object->set_iso_day(reference_iso_day);
  // 11. Return object.
  return object;
}

MaybeHandle<JSTemporalPlainYearMonth> CreateTemporalYearMonth(
    Isolate* isolate, int32_t iso_year, int32_t iso_month,
    Handle<JSReceiver> calendar, int32_t reference_iso_day) {
  TEMPORAL_ENTER_FUNC();
  return CreateTemporalYearMonth(isolate, CONSTRUCTOR(plain_year_month),
                                 CONSTRUCTOR(plain_year_month), iso_year,
                                 iso_month, calendar, reference_iso_day);
}

// #sec-temporal-createtemporalzoneddatetime
MaybeHandle<JSTemporalZonedDateTime> CreateTemporalZonedDateTime(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    Handle<BigInt> epoch_nanoseconds, Handle<JSReceiver> time_zone,
    Handle<JSReceiver> calendar) {
  TEMPORAL_ENTER_FUNC();
  // 1. Assert: Type(epochNanoseconds) is BigInt.
  // 2. Assert: ! IsValidEpochNanoseconds(epochNanoseconds) is true.
  DCHECK(IsValidEpochNanoseconds(isolate, epoch_nanoseconds));
  // 3. Assert: Type(timeZone) is Object.
  // 4. Assert: Type(calendar) is Object.
  // 5. If newTarget is not present, set it to %Temporal.ZonedDateTime%.
  // 6. Let object be ? OrdinaryCreateFromConstructor(newTarget,
  // "%Temporal.ZonedDateTime.prototype%", «
  // [[InitializedTemporalZonedDateTime]], [[Nanoseconds]], [[TimeZone]],
  // [[Calendar]] »).
  ORDINARY_CREATE_FROM_CONSTRUCTOR(object, target, new_target,
                                   JSTemporalZonedDateTime)
  // 7. Set object.[[Nanoseconds]] to epochNanoseconds.
  object->set_nanoseconds(*epoch_nanoseconds);
  // 8. Set object.[[TimeZone]] to timeZone.
  object->set_time_zone(*time_zone);
  // 9. Set object.[[Calendar]] to calendar.
  object->set_calendar(*calendar);
  // 10. Return object.
  return object;
}

MaybeHandle<JSTemporalZonedDateTime> CreateTemporalZonedDateTime(
    Isolate* isolate, Handle<BigInt> epoch_nanoseconds,
    Handle<JSReceiver> time_zone, Handle<JSReceiver> calendar) {
  TEMPORAL_ENTER_FUNC();
  return CreateTemporalZonedDateTime(isolate, CONSTRUCTOR(zoned_date_time),
                                     CONSTRUCTOR(zoned_date_time),
                                     epoch_nanoseconds, time_zone, calendar);
}

inline double NormalizeMinusZero(double v) { return IsMinusZero(v) ? 0 : v; }

// #sec-temporal-createdatedurationrecord
Maybe<DateDurationRecord> DateDurationRecord::Create(
    Isolate* isolate, double years, double months, double weeks, double days) {
  // 1. If ! IsValidDuration(years, months, weeks, days, 0, 0, 0, 0, 0, 0) is
  // false, throw a RangeError exception.
  if (!IsValidDuration(isolate,
                       {years, months, weeks, {days, 0, 0, 0, 0, 0, 0}})) {
    THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                 NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                                 Nothing<DateDurationRecord>());
  }
  // 2. Return the Record { [[Years]]: ℝ(𝔽(years)), [[Months]]: ℝ(𝔽(months)),
  // [[Weeks]]: ℝ(𝔽(weeks)), [[Days]]: ℝ(𝔽(days)) }.
  DateDurationRecord record = {years, months, weeks, days};
  return Just(record);
}

// #sec-temporal-createtimedurationrecord
Maybe<TimeDurationRecord> TimeDurationRecord::Create(
    Isolate* isolate, double days, double hours, double minutes, double seconds,
    double milliseconds, double microseconds, double nanoseconds) {
  // 1. If ! IsValidDuration(0, 0, 0, days, hours, minutes, seconds,
  // milliseconds, microseconds, nanoseconds) is false, throw a RangeError
  // exception.
  TimeDurationRecord record = {days,         hours,        minutes,    seconds,
                               milliseconds, microseconds, nanoseconds};
  if (!IsValidDuration(isolate, {0, 0, 0, record})) {
    THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                 NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                                 Nothing<TimeDurationRecord>());
  }
  // 2. Return the Record { [[Days]]: ℝ(𝔽(days)), [[Hours]]: ℝ(𝔽(hours)),
  // [[Minutes]]: ℝ(𝔽(minutes)), [[Seconds]]: ℝ(𝔽(seconds)), [[Milliseconds]]:
  // ℝ(𝔽(milliseconds)), [[Microseconds]]: ℝ(𝔽(microseconds)), [[Nanoseconds]]:
  // ℝ(𝔽(nanoseconds)) }.
  return Just(record);
}

// #sec-temporal-createdurationrecord
Maybe<DurationRecord> DurationRecord::Create(
    Isolate* isolate, double years, double months, double weeks, double days,
    double hours, double minutes, double seconds, double milliseconds,
    double microseconds, double nanoseconds) {
  // 1. If ! IsValidDuration(years, months, weeks, days, hours, minutes,
  // seconds, milliseconds, microseconds, nanoseconds) is false, throw a
  // RangeError exception.
  DurationRecord record = {
      years,
      months,
      weeks,
      {days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds}};
  if (!IsValidDuration(isolate, record)) {
    THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                 NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                                 Nothing<DurationRecord>());
  }
  // 2. Return the Record { [[Years]]: ℝ(𝔽(years)), [[Months]]: ℝ(𝔽(months)),
  // [[Weeks]]: ℝ(𝔽(weeks)), [[Days]]: ℝ(𝔽(days)), [[Hours]]: ℝ(𝔽(hours)),
  // [[Minutes]]: ℝ(𝔽(minutes)), [[Seconds]]: ℝ(𝔽(seconds)), [[Milliseconds]]:
  // ℝ(𝔽(milliseconds)), [[Microseconds]]: ℝ(𝔽(microseconds)), [[Nanoseconds]]:
  // ℝ(𝔽(nanoseconds)) }.
  return Just(record);
}

// #sec-temporal-createtemporalduration
MaybeHandle<JSTemporalDuration> CreateTemporalDuration(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    const DurationRecord& duration) {
  TEMPORAL_ENTER_FUNC();
  Factory* factory = isolate->factory();
  // 1. If ! IsValidDuration(years, months, weeks, days, hours, minutes,
  // seconds, milliseconds, microseconds, nanoseconds) is false, throw a
  // RangeError exception.
  if (!IsValidDuration(isolate, duration)) {
    THROW_INVALID_RANGE(JSTemporalDuration);
  }

  // 2. If newTarget is not present, set it to %Temporal.Duration%.
  // 3. Let object be ? OrdinaryCreateFromConstructor(newTarget,
  // "%Temporal.Duration.prototype%", « [[InitializedTemporalDuration]],
  // [[Years]], [[Months]], [[Weeks]], [[Days]], [[Hours]], [[Minutes]],
  // [[Seconds]], [[Milliseconds]], [[Microseconds]], [[Nanoseconds]] »).
  const TimeDurationRecord& time_duration = duration.time_duration;
  Handle<Object> years = factory->NewNumber(NormalizeMinusZero(duration.years));
  Handle<Object> months =
      factory->NewNumber(NormalizeMinusZero(duration.months));
  Handle<Object> weeks = factory->NewNumber(NormalizeMinusZero(duration.weeks));
  Handle<Object> days =
      factory->NewNumber(NormalizeMinusZero(time_duration.days));
  Handle<Object> hours =
      factory->NewNumber(NormalizeMinusZero(time_duration.hours));
  Handle<Object> minutes =
      factory->NewNumber(NormalizeMinusZero(time_duration.minutes));
  Handle<Object> seconds =
      factory->NewNumber(NormalizeMinusZero(time_duration.seconds));
  Handle<Object> milliseconds =
      factory->NewNumber(NormalizeMinusZero(time_duration.milliseconds));
  Handle<Object> microseconds =
      factory->NewNumber(NormalizeMinusZero(time_duration.microseconds));
  Handle<Object> nanoseconds =
      factory->NewNumber(NormalizeMinusZero(time_duration.nanoseconds));
  ORDINARY_CREATE_FROM_CONSTRUCTOR(object, target, new_target,
                                   JSTemporalDuration)
  // 4. Set object.[[Years]] to ℝ(𝔽(years)).
  object->set_years(*years);
  // 5. Set object.[[Months]] to ℝ(𝔽(months)).
  object->set_months(*months);
  // 6. Set object.[[Weeks]] to ℝ(𝔽(weeks)).
  object->set_weeks(*weeks);
  // 7. Set object.[[Days]] to ℝ(𝔽(days)).
  object->set_days(*days);
  // 8. Set object.[[Hours]] to ℝ(𝔽(hours)).
  object->set_hours(*hours);
  // 9. Set object.[[Minutes]] to ℝ(𝔽(minutes)).
  object->set_minutes(*minutes);
  // 10. Set object.[[Seconds]] to ℝ(𝔽(seconds)).
  object->set_seconds(*seconds);
  // 11. Set object.[[Milliseconds]] to ℝ(𝔽(milliseconds)).
  object->set_milliseconds(*milliseconds);
  // 12. Set object.[[Microseconds]] to ℝ(𝔽(microseconds)).
  object->set_microseconds(*microseconds);
  // 13. Set object.[[Nanoseconds]] to ℝ(𝔽(nanoseconds)).
  object->set_nanoseconds(*nanoseconds);
  // 14. Return object.
  return object;
}

MaybeHandle<JSTemporalDuration> CreateTemporalDuration(
    Isolate* isolate, const DurationRecord& duration) {
  TEMPORAL_ENTER_FUNC();
  return CreateTemporalDuration(isolate, CONSTRUCTOR(duration),
                                CONSTRUCTOR(duration), duration);
}

}  // namespace

namespace temporal {

// #sec-temporal-createtemporalinstant
MaybeHandle<JSTemporalInstant> CreateTemporalInstant(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    Handle<BigInt> epoch_nanoseconds) {
  TEMPORAL_ENTER_FUNC();
  // 1. Assert: Type(epochNanoseconds) is BigInt.
  // 2. Assert: ! IsValidEpochNanoseconds(epochNanoseconds) is true.
  DCHECK(IsValidEpochNanoseconds(isolate, epoch_nanoseconds));

  // 4. Let object be ? OrdinaryCreateFromConstructor(newTarget,
  // "%Temporal.Instant.prototype%", « [[InitializedTemporalInstant]],
  // [[Nanoseconds]] »).
  ORDINARY_CREATE_FROM_CONSTRUCTOR(object, target, new_target,
                                   JSTemporalInstant)
  // 5. Set object.[[Nanoseconds]] to ns.
  object->set_nanoseconds(*epoch_nanoseconds);
  return object;
}

MaybeHandle<JSTemporalInstant> CreateTemporalInstant(
    Isolate* isolate, Handle<BigInt> epoch_nanoseconds) {
  TEMPORAL_ENTER_FUNC();
  return CreateTemporalInstant(isolate, CONSTRUCTOR(instant),
                               CONSTRUCTOR(instant), epoch_nanoseconds);
}

}  // namespace temporal

namespace {

MaybeHandle<JSTemporalTimeZone> CreateTemporalTimeZoneFromIndex(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    int32_t index) {
  TEMPORAL_ENTER_FUNC();
  ORDINARY_CREATE_FROM_CONSTRUCTOR(object, target, new_target,
                                   JSTemporalTimeZone)
  object->set_flags(0);
  object->set_details(0);

  object->set_is_offset(false);
  object->set_offset_milliseconds_or_time_zone_index(index);
  return object;
}

MaybeHandle<JSTemporalTimeZone> CreateTemporalTimeZoneUTC(
    Isolate* isolate, Handle<JSFunction> target,
    Handle<HeapObject> new_target) {
  TEMPORAL_ENTER_FUNC();
  return CreateTemporalTimeZoneFromIndex(isolate, target, new_target, 0);
}

bool IsUTC(Isolate* isolate, Handle<String> time_zone);

// #sec-temporal-createtemporaltimezone
MaybeHandle<JSTemporalTimeZone> CreateTemporalTimeZone(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    Handle<String> identifier) {
  TEMPORAL_ENTER_FUNC();
  // 1. If newTarget is not present, set it to %Temporal.TimeZone%.
  // 2. Let object be ? OrdinaryCreateFromConstructor(newTarget,
  // "%Temporal.TimeZone.prototype%", « [[InitializedTemporalTimeZone]],
  // [[Identifier]], [[OffsetNanoseconds]] »).
  // 3. Set object.[[Identifier]] to identifier.
  if (IsUTC(isolate, identifier)) {
    return CreateTemporalTimeZoneUTC(isolate, target, new_target);
  }
#ifdef V8_INTL_SUPPORT
  int32_t time_zone_index = Intl::GetTimeZoneIndex(isolate, identifier);
  if (time_zone_index >= 0) {
    return CreateTemporalTimeZoneFromIndex(isolate, target, new_target,
                                           time_zone_index);
  }
#endif  // V8_INTL_SUPPORT

  // 4. If identifier satisfies the syntax of a TimeZoneNumericUTCOffset
  // (see 13.33), then a. Set object.[[OffsetNanoseconds]] to !
  // ParseTimeZoneOffsetString(identifier).
  // 5. Else,
  // a. Assert: ! CanonicalizeTimeZoneName(identifier) is identifier.
  // b. Set object.[[OffsetNanoseconds]] to undefined.
  // 6. Return object.
  int64_t offset_nanoseconds;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, offset_nanoseconds,
      ParseTimeZoneOffsetString(isolate, identifier, false),
      Handle<JSTemporalTimeZone>());

  ORDINARY_CREATE_FROM_CONSTRUCTOR(object, target, new_target,
                                   JSTemporalTimeZone)
  object->set_flags(0);
  object->set_details(0);

  object->set_is_offset(true);
  object->set_offset_nanoseconds(offset_nanoseconds);
  return object;
}

MaybeHandle<JSTemporalTimeZone> CreateTemporalTimeZoneDefaultTarget(
    Isolate* isolate, Handle<String> identifier) {
  TEMPORAL_ENTER_FUNC();
  return CreateTemporalTimeZone(isolate, CONSTRUCTOR(time_zone),
                                CONSTRUCTOR(time_zone), identifier);
}

}  // namespace

namespace temporal {
MaybeHandle<JSTemporalTimeZone> CreateTemporalTimeZone(
    Isolate* isolate, Handle<String> identifier) {
  return CreateTemporalTimeZoneDefaultTarget(isolate, identifier);
}
}  // namespace temporal

namespace {

// #sec-temporal-systeminstant
MaybeHandle<JSTemporalInstant> SystemInstant(Isolate* isolate) {
  TEMPORAL_ENTER_FUNC();
  // 1. Let ns be ! SystemUTCEpochNanoseconds().
  Handle<BigInt> ns;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, ns, SystemUTCEpochNanoseconds(isolate),
                             JSTemporalInstant);
  // 2. Return ? CreateTemporalInstant(ns).
  return temporal::CreateTemporalInstant(isolate, ns);
}

// #sec-temporal-systemtimezone
MaybeHandle<JSTemporalTimeZone> SystemTimeZone(Isolate* isolate) {
  TEMPORAL_ENTER_FUNC();
  Handle<String> default_time_zone;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, default_time_zone,
                             DefaultTimeZone(isolate), JSTemporalTimeZone);
  return temporal::CreateTemporalTimeZone(isolate, default_time_zone);
}

DateTimeRecordCommon GetISOPartsFromEpoch(Isolate* isolate,
                                          Handle<BigInt> epoch_nanoseconds) {
  TEMPORAL_ENTER_FUNC();
  DateTimeRecordCommon result;
  // 1. Let remainderNs be epochNanoseconds modulo 10^6.
  Handle<BigInt> million = BigInt::FromInt64(isolate, 1000000);
  Handle<BigInt> remainder_ns =
      BigInt::Remainder(isolate, epoch_nanoseconds, million).ToHandleChecked();
  // Need to do some remainder magic to negative remainder.
  if (remainder_ns->IsNegative()) {
    remainder_ns =
        BigInt::Add(isolate, remainder_ns, million).ToHandleChecked();
  }

  // 2. Let epochMilliseconds be (epochNanoseconds − remainderNs) / 10^6.
  int64_t epoch_milliseconds =
      BigInt::Divide(isolate,
                     BigInt::Subtract(isolate, epoch_nanoseconds, remainder_ns)
                         .ToHandleChecked(),
                     million)
          .ToHandleChecked()
          ->AsInt64();
  int year = 0;
  int month = 0;
  int day = 0;
  int wday = 0;
  int hour = 0;
  int min = 0;
  int sec = 0;
  int ms = 0;
  isolate->date_cache()->BreakDownTime(epoch_milliseconds, &year, &month, &day,
                                       &wday, &hour, &min, &sec, &ms);

  // 3. Let year be ! YearFromTime(epochMilliseconds).
  result.date.year = year;
  // 4. Let month be ! MonthFromTime(epochMilliseconds) + 1.
  result.date.month = month + 1;
  DCHECK_GE(result.date.month, 1);
  DCHECK_LE(result.date.month, 12);
  // 5. Let day be ! DateFromTime(epochMilliseconds).
  result.date.day = day;
  DCHECK_GE(result.date.day, 1);
  DCHECK_LE(result.date.day, 31);
  // 6. Let hour be ! HourFromTime(epochMilliseconds).
  result.time.hour = hour;
  DCHECK_GE(result.time.hour, 0);
  DCHECK_LE(result.time.hour, 23);
  // 7. Let minute be ! MinFromTime(epochMilliseconds).
  result.time.minute = min;
  DCHECK_GE(result.time.minute, 0);
  DCHECK_LE(result.time.minute, 59);
  // 8. Let second be ! SecFromTime(epochMilliseconds).
  result.time.second = sec;
  DCHECK_GE(result.time.second, 0);
  DCHECK_LE(result.time.second, 59);
  // 9. Let millisecond be ! msFromTime(epochMilliseconds).
  result.time.millisecond = ms;
  DCHECK_GE(result.time.millisecond, 0);
  DCHECK_LE(result.time.millisecond, 999);
  // 10. Let microsecond be floor(remainderNs / 1000) modulo 1000.
  int64_t remainder = remainder_ns->AsInt64();
  result.time.microsecond = (remainder / 1000) % 1000;
  DCHECK_GE(result.time.microsecond, 0);
  DCHECK_LE(result.time.microsecond, 999);
  // 11. Let nanosecond be remainderNs modulo 1000.
  result.time.nanosecond = remainder % 1000;
  DCHECK_GE(result.time.nanosecond, 0);
  DCHECK_LE(result.time.nanosecond, 999);
  return result;
}

// #sec-temporal-balanceisodatetime
DateTimeRecordCommon BalanceISODateTime(Isolate* isolate,
                                        const DateTimeRecordCommon& date_time) {
  TEMPORAL_ENTER_FUNC();
  // 1. Assert: year, month, day, hour, minute, second, millisecond,
  // microsecond, and nanosecond are integers.
  // 2. Let balancedTime be ! BalanceTime(hour, minute, second, millisecond,
  // microsecond, nanosecond).
  DateTimeRecordCommon balanced_time = BalanceTime(date_time.time);
  // 3. Let balancedDate be ! BalanceISODate(year, month, day +
  // balancedTime.[[Days]]).
  DateRecordCommon added_date = date_time.date;
  added_date.day += balanced_time.date.day;
  DateRecordCommon balanced_date = BalanceISODate(isolate, added_date);
  // 4. Return the Record { [[Year]]: balancedDate.[[Year]], [[Month]]:
  // balancedDate.[[Month]], [[Day]]: balancedDate.[[Day]], [[Hour]]:
  // balancedTime.[[Hour]], [[Minute]]: balancedTime.[[Minute]], [[Second]]:
  // balancedTime.[[Second]], [[Millisecond]]: balancedTime.[[Millisecond]],
  // [[Microsecond]]: balancedTime.[[Microsecond]], [[Nanosecond]]:
  // balancedTime.[[Nanosecond]] }.
  return {balanced_date, balanced_time.time};
}

}  // namespace

namespace temporal {
MaybeHandle<JSTemporalPlainDateTime> BuiltinTimeZoneGetPlainDateTimeFor(
    Isolate* isolate, Handle<JSReceiver> time_zone,
    Handle<JSTemporalInstant> instant, Handle<JSReceiver> calendar,
    const char* method_name) {
  TEMPORAL_ENTER_FUNC();
  // 1. Let offsetNanoseconds be ? GetOffsetNanosecondsFor(timeZone, instant).
  int64_t offset_nanoseconds;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, offset_nanoseconds,
      GetOffsetNanosecondsFor(isolate, time_zone, instant, method_name),
      Handle<JSTemporalPlainDateTime>());
  // 2. Let result be ! GetISOPartsFromEpoch(instant.[[Nanoseconds]]).
  DateTimeRecordCommon result = GetISOPartsFromEpoch(
      isolate, Handle<BigInt>(instant->nanoseconds(), isolate));

  // 3. Set result to ! BalanceISODateTime(result.[[Year]], result.[[Month]],
  // result.[[Day]], result.[[Hour]], result.[[Minute]], result.[[Second]],
  // result.[[Millisecond]], result.[[Microsecond]], result.[[Nanosecond]] +
  // offsetNanoseconds).

  // Note: Since offsetNanoseconds is bounded 86400x 10^9, the
  // result of result.[[Nanosecond]] + offsetNanoseconds may overflow int32_t
  // Therefore we distribute the sum to other fields below to make sure it won't
  // overflow each of the int32_t fields. But it will leave each field to be
  // balanced by BalanceISODateTime
  result.time.nanosecond += offset_nanoseconds % 1000;
  result.time.microsecond += (offset_nanoseconds / 1000) % 1000;
  result.time.millisecond += (offset_nanoseconds / 1000000L) % 1000;
  result.time.second += (offset_nanoseconds / 1000000000L) % 60;
  result.time.minute += (offset_nanoseconds / 60000000000L) % 60;
  result.time.hour += (offset_nanoseconds / 3600000000000L) % 24;
  result.date.day += (offset_nanoseconds / 86400000000000L);

  result = BalanceISODateTime(isolate, result);
  // 4. Return ? CreateTemporalDateTime(result.[[Year]], result.[[Month]],
  // result.[[Day]], result.[[Hour]], result.[[Minute]], result.[[Second]],
  // result.[[Millisecond]], result.[[Microsecond]], result.[[Nanosecond]],
  // calendar).
  return temporal::CreateTemporalDateTime(isolate, result, calendar);
}

}  // namespace temporal

namespace {
// #sec-temporal-getpossibleinstantsfor
MaybeHandle<FixedArray> GetPossibleInstantsFor(Isolate* isolate,
                                               Handle<JSReceiver> time_zone,
                                               Handle<Object> date_time) {
  TEMPORAL_ENTER_FUNC();
  // 1. Let possibleInstants be ? Invoke(timeZone, "getPossibleInstantsFor", «
  // dateTime »).
  Handle<Object> function;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, function,
      Object::GetProperty(isolate, time_zone,
                          isolate->factory()->getPossibleInstantsFor_string()),
      FixedArray);
  if (!function->IsCallable()) {
    THROW_NEW_ERROR(
        isolate,
        NewTypeError(MessageTemplate::kCalledNonCallable,
                     isolate->factory()->getPossibleInstantsFor_string()),
        FixedArray);
  }
  Handle<Object> possible_instants;
  {
    Handle<Object> argv[] = {date_time};
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, possible_instants,
        Execution::Call(isolate, function, time_zone, arraysize(argv), argv),
        FixedArray);
  }

  // Step 4-6 of GetPossibleInstantsFor is implemented inside
  // temporal_instant_fixed_array_from_iterable.
  {
    Handle<Object> argv[] = {possible_instants};
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, possible_instants,
        Execution::CallBuiltin(
            isolate, isolate->temporal_instant_fixed_array_from_iterable(),
            possible_instants, arraysize(argv), argv),
        FixedArray);
  }
  DCHECK(possible_instants->IsFixedArray());
  // 7. Return list.
  return Handle<FixedArray>::cast(possible_instants);
}

// #sec-temporal-disambiguatepossibleinstants
MaybeHandle<JSTemporalInstant> DisambiguatePossibleInstants(
    Isolate* isolate, Handle<FixedArray> possible_instants,
    Handle<JSReceiver> time_zone, Handle<Object> date_time_obj,
    Disambiguation disambiguation, const char* method_name) {
  TEMPORAL_ENTER_FUNC();
  // 1. Assert: dateTime has an [[InitializedTemporalDateTime]] internal slot.
  DCHECK(date_time_obj->IsJSTemporalPlainDateTime());
  Handle<JSTemporalPlainDateTime> date_time =
      Handle<JSTemporalPlainDateTime>::cast(date_time_obj);

  // 2. Let n be possibleInstants's length.
  int32_t n = possible_instants->length();

  // 3. If n = 1, then
  if (n == 1) {
    // a. Return possibleInstants[0].
    Handle<Object> ret_obj = FixedArray::get(*possible_instants, 0, isolate);
    DCHECK(ret_obj->IsJSTemporalInstant());
    return Handle<JSTemporalInstant>::cast(ret_obj);
  }
  // 4. If n ≠ 0, then
  if (n != 0) {
    // a. If disambiguation is "earlier" or "compatible", then
    if (disambiguation == Disambiguation::kEarlier ||
        disambiguation == Disambiguation::kCompatible) {
      // i. Return possibleInstants[0].
      Handle<Object> ret_obj = FixedArray::get(*possible_instants, 0, isolate);
      DCHECK(ret_obj->IsJSTemporalInstant());
      return Handle<JSTemporalInstant>::cast(ret_obj);
    }
    // b. If disambiguation is "later", then
    if (disambiguation == Disambiguation::kLater) {
      // i. Return possibleInstants[n − 1].
      Handle<Object> ret_obj =
          FixedArray::get(*possible_instants, n - 1, isolate);
      DCHECK(ret_obj->IsJSTemporalInstant());
      return Handle<JSTemporalInstant>::cast(ret_obj);
    }
    // c. Assert: disambiguation is "reject".
    DCHECK_EQ(disambiguation, Disambiguation::kReject);
    // d. Throw a RangeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                    JSTemporalInstant);
  }
  // 5. Assert: n = 0.
  DCHECK_EQ(n, 0);
  // 6. If disambiguation is "reject", then
  if (disambiguation == Disambiguation::kReject) {
    // a. Throw a RangeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                    JSTemporalInstant);
  }
  // 7. Let epochNanoseconds be ! GetEpochFromISOParts(dateTime.[[ISOYear]],
  // dateTime.[[ISOMonth]], dateTime.[[ISODay]], dateTime.[[ISOHour]],
  // dateTime.[[ISOMinute]], dateTime.[[ISOSecond]],
  // dateTime.[[ISOMillisecond]], dateTime.[[ISOMicrosecond]],
  // dateTime.[[ISONanosecond]]).
  Handle<BigInt> epoch_nanoseconds = GetEpochFromISOParts(
      isolate,
      {{date_time->iso_year(), date_time->iso_month(), date_time->iso_day()},
       {date_time->iso_hour(), date_time->iso_minute(), date_time->iso_second(),
        date_time->iso_millisecond(), date_time->iso_microsecond(),
        date_time->iso_nanosecond()}});

  // 8. Let dayBefore be ! CreateTemporalInstant(epochNanoseconds − 8.64 ×
  // 10^13).
  Handle<BigInt> one_day_in_ns = BigInt::FromUint64(isolate, 86400000000000ULL);
  Handle<BigInt> day_before_ns;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, day_before_ns,
      BigInt::Subtract(isolate, epoch_nanoseconds, one_day_in_ns),
      JSTemporalInstant);
  Handle<JSTemporalInstant> day_before;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, day_before,
      temporal::CreateTemporalInstant(isolate, day_before_ns),
      JSTemporalInstant);
  // 9. Let dayAfter be ! CreateTemporalInstant(epochNanoseconds + 8.64 ×
  // 10^13).
  Handle<BigInt> day_after_ns;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, day_after_ns,
      BigInt::Add(isolate, epoch_nanoseconds, one_day_in_ns),
      JSTemporalInstant);
  Handle<JSTemporalInstant> day_after;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, day_after,
      temporal::CreateTemporalInstant(isolate, day_after_ns),
      JSTemporalInstant);
  // 10. Let offsetBefore be ? GetOffsetNanosecondsFor(timeZone, dayBefore).
  int64_t offset_before;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, offset_before,
      GetOffsetNanosecondsFor(isolate, time_zone, day_before, method_name),
      Handle<JSTemporalInstant>());
  // 11. Let offsetAfter be ? GetOffsetNanosecondsFor(timeZone, dayAfter).
  int64_t offset_after;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, offset_after,
      GetOffsetNanosecondsFor(isolate, time_zone, day_after, method_name),
      Handle<JSTemporalInstant>());

  // 12. Let nanoseconds be offsetAfter − offsetBefore.
  double nanoseconds = offset_after - offset_before;

  // 13. If disambiguation is "earlier", then
  if (disambiguation == Disambiguation::kEarlier) {
    // a. Let earlier be ? AddDateTime(dateTime.[[ISOYear]],
    // dateTime.[[ISOMonth]], dateTime.[[ISODay]], dateTime.[[ISOHour]],
    // dateTime.[[ISOMinute]], dateTime.[[ISOSecond]],
    // dateTime.[[ISOMillisecond]],
    // dateTime.[[ISOMicrosecond]], dateTime.[[ISONanosecond]],
    // dateTime.[[Calendar]], 0, 0, 0, 0, 0, 0, 0, 0, 0, −nanoseconds,
    // undefined).
    DateTimeRecordCommon earlier;
    MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, earlier,
        AddDateTime(
            isolate,
            {{date_time->iso_year(), date_time->iso_month(),
              date_time->iso_day()},
             {date_time->iso_hour(), date_time->iso_minute(),
              date_time->iso_second(), date_time->iso_millisecond(),
              date_time->iso_microsecond(), date_time->iso_nanosecond()}},
            handle(date_time->calendar(), isolate),
            {0, 0, 0, {0, 0, 0, 0, 0, 0, -nanoseconds}},
            isolate->factory()->undefined_value()),
        Handle<JSTemporalInstant>());
    // See https://github.com/tc39/proposal-temporal/issues/1816
    // b. Let earlierDateTime be ? CreateTemporalDateTime(earlier.[[Year]],
    // earlier.[[Month]], earlier.[[Day]], earlier.[[Hour]], earlier.[[Minute]],
    // earlier.[[Second]], earlier.[[Millisecond]], earlier.[[Microsecond]],
    // earlier.[[Nanosecond]], dateTime.[[Calendar]]).
    Handle<JSTemporalPlainDateTime> earlier_date_time;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, earlier_date_time,
        temporal::CreateTemporalDateTime(
            isolate, earlier, handle(date_time->calendar(), isolate)),
        JSTemporalInstant);

    // c. Set possibleInstants to ? GetPossibleInstantsFor(timeZone,
    // earlierDateTime).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, possible_instants,
        GetPossibleInstantsFor(isolate, time_zone, earlier_date_time),
        JSTemporalInstant);

    // d. If possibleInstants is empty, throw a RangeError exception.
    if (possible_instants->length() == 0) {
      THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                      JSTemporalInstant);
    }
    // e. Return possibleInstants[0].
    Handle<Object> ret_obj = FixedArray::get(*possible_instants, 0, isolate);
    DCHECK(ret_obj->IsJSTemporalInstant());
    return Handle<JSTemporalInstant>::cast(ret_obj);
  }
  // 14. Assert: disambiguation is "compatible" or "later".
  DCHECK(disambiguation == Disambiguation::kCompatible ||
         disambiguation == Disambiguation::kLater);
  // 15. Let later be ? AddDateTime(dateTime.[[ISOYear]], dateTime.[[ISOMonth]],
  // dateTime.[[ISODay]], dateTime.[[ISOHour]], dateTime.[[ISOMinute]],
  // dateTime.[[ISOSecond]], dateTime.[[ISOMillisecond]],
  // dateTime.[[ISOMicrosecond]], dateTime.[[ISONanosecond]],
  // dateTime.[[Calendar]], 0, 0, 0, 0, 0, 0, 0, 0, 0, nanoseconds, undefined).
  DateTimeRecordCommon later;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, later,
      AddDateTime(isolate,
                  {{date_time->iso_year(), date_time->iso_month(),
                    date_time->iso_day()},
                   {date_time->iso_hour(), date_time->iso_minute(),
                    date_time->iso_second(), date_time->iso_millisecond(),
                    date_time->iso_microsecond(), date_time->iso_nanosecond()}},
                  handle(date_time->calendar(), isolate),
                  {0, 0, 0, {0, 0, 0, 0, 0, 0, nanoseconds}},
                  isolate->factory()->undefined_value()),
      Handle<JSTemporalInstant>());

  // See https://github.com/tc39/proposal-temporal/issues/1816
  // 16. Let laterDateTime be ? CreateTemporalDateTime(later.[[Year]],
  // later.[[Month]], later.[[Day]], later.[[Hour]], later.[[Minute]],
  // later.[[Second]], later.[[Millisecond]], later.[[Microsecond]],
  // later.[[Nanosecond]], dateTime.[[Calendar]]).

  Handle<JSTemporalPlainDateTime> later_date_time;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, later_date_time,
      temporal::CreateTemporalDateTime(isolate, later,
                                       handle(date_time->calendar(), isolate)),
      JSTemporalInstant);
  // 17. Set possibleInstants to ? GetPossibleInstantsFor(timeZone,
  // laterDateTime).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, possible_instants,
      GetPossibleInstantsFor(isolate, time_zone, later_date_time),
      JSTemporalInstant);
  // 18. Set n to possibleInstants's length.
  n = possible_instants->length();
  // 19. If n = 0, throw a RangeError exception.
  if (n == 0) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                    JSTemporalInstant);
  }
  // 20. Return possibleInstants[n − 1].
  Handle<Object> ret_obj = FixedArray::get(*possible_instants, n - 1, isolate);
  DCHECK(ret_obj->IsJSTemporalInstant());
  return Handle<JSTemporalInstant>::cast(ret_obj);
}

// #sec-temporal-gettemporalcalendarwithisodefault
MaybeHandle<JSReceiver> GetTemporalCalendarWithISODefault(
    Isolate* isolate, Handle<JSReceiver> item, const char* method_name) {
  TEMPORAL_ENTER_FUNC();

  Factory* factory = isolate->factory();
  // 1. If item has an [[InitializedTemporalDate]],
  // [[InitializedTemporalDateTime]], [[InitializedTemporalMonthDay]],
  // [[InitializedTemporalTime]], [[InitializedTemporalYearMonth]], or
  // [[InitializedTemporalZonedDateTime]] internal slot, then a. Return
  // item.[[Calendar]].
  if (item->IsJSTemporalPlainDate()) {
    return handle(Handle<JSTemporalPlainDate>::cast(item)->calendar(), isolate);
  }
  if (item->IsJSTemporalPlainDateTime()) {
    return handle(Handle<JSTemporalPlainDateTime>::cast(item)->calendar(),
                  isolate);
  }
  if (item->IsJSTemporalPlainMonthDay()) {
    return handle(Handle<JSTemporalPlainMonthDay>::cast(item)->calendar(),
                  isolate);
  }
  if (item->IsJSTemporalPlainTime()) {
    return handle(Handle<JSTemporalPlainTime>::cast(item)->calendar(), isolate);
  }
  if (item->IsJSTemporalPlainYearMonth()) {
    return handle(Handle<JSTemporalPlainYearMonth>::cast(item)->calendar(),
                  isolate);
  }
  if (item->IsJSTemporalZonedDateTime()) {
    return handle(Handle<JSTemporalZonedDateTime>::cast(item)->calendar(),
                  isolate);
  }

  // 2. Let calendar be ? Get(item, "calendar").
  Handle<Object> calendar;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, calendar,
      JSReceiver::GetProperty(isolate, item, factory->calendar_string()),
      JSReceiver);
  // 3. Return ? ToTemporalCalendarWithISODefault(calendar).
  return ToTemporalCalendarWithISODefault(isolate, calendar, method_name);
}

enum class RequiredFields { kNone, kTimeZone, kTimeZoneAndOffset, kDay };

// The common part of PrepareTemporalFields and PreparePartialTemporalFields
// #sec-temporal-preparetemporalfields
// #sec-temporal-preparepartialtemporalfields
V8_WARN_UNUSED_RESULT MaybeHandle<JSObject> PrepareTemporalFieldsOrPartial(
    Isolate* isolate, Handle<JSReceiver> fields, Handle<FixedArray> field_names,
    RequiredFields required, bool partial) {
  TEMPORAL_ENTER_FUNC();

  Factory* factory = isolate->factory();
  // 1. Assert: Type(fields) is Object.
  // 2. Let result be ! OrdinaryObjectCreate(%Object.prototype%).
  Handle<JSObject> result =
      isolate->factory()->NewJSObject(isolate->object_function());
  // 3. For each value property of fieldNames, do
  int length = field_names->length();
  bool any = false;
  for (int i = 0; i < length; i++) {
    Handle<Object> property_obj = Handle<Object>(field_names->get(i), isolate);
    Handle<String> property = Handle<String>::cast(property_obj);
    // a. Let value be ? Get(fields, property).
    Handle<Object> value;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, value, JSReceiver::GetProperty(isolate, fields, property),
        JSObject);

    // b. If value is undefined, then
    if (value->IsUndefined()) {
      // This part is only for PrepareTemporalFields
      // Skip for the case of PreparePartialTemporalFields.
      if (partial) continue;

      // i. If requiredFields contains property, then
      if ((required == RequiredFields::kDay &&
           String::Equals(isolate, property, factory->day_string())) ||
          ((required == RequiredFields::kTimeZone ||
            required == RequiredFields::kTimeZoneAndOffset) &&
           String::Equals(isolate, property, factory->timeZone_string())) ||
          (required == RequiredFields::kTimeZoneAndOffset &&
           String::Equals(isolate, property, factory->offset_string()))) {
        // 1. Throw a TypeError exception.
        THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_TYPE_ERROR(),
                        JSObject);
      }
      // ii. Else,
      // 1. If property is in the Property column of Table 13, then
      // a. Set value to the corresponding Default value of the same row.
      if (String::Equals(isolate, property, factory->hour_string()) ||
          String::Equals(isolate, property, factory->minute_string()) ||
          String::Equals(isolate, property, factory->second_string()) ||
          String::Equals(isolate, property, factory->millisecond_string()) ||
          String::Equals(isolate, property, factory->microsecond_string()) ||
          String::Equals(isolate, property, factory->nanosecond_string())) {
        value = Handle<Object>(Smi::zero(), isolate);
      }
    } else {
      // For both PrepareTemporalFields and PreparePartialTemporalFields
      any = partial;
      // c. Else,
      // i. If property is in the Property column of Table 13 and there is a
      // Conversion value in the same row, then
      // 1. Let Conversion represent the abstract operation named by the
      // Conversion value of the same row.
      // 2. Set value to ? Conversion(value).
      if (String::Equals(isolate, property, factory->month_string()) ||
          String::Equals(isolate, property, factory->day_string())) {
        ASSIGN_RETURN_ON_EXCEPTION(isolate, value,
                                   ToPositiveInteger(isolate, value), JSObject);
      } else if (String::Equals(isolate, property, factory->year_string()) ||
                 String::Equals(isolate, property, factory->hour_string()) ||
                 String::Equals(isolate, property, factory->minute_string()) ||
                 String::Equals(isolate, property, factory->second_string()) ||
                 String::Equals(isolate, property,
                                factory->millisecond_string()) ||
                 String::Equals(isolate, property,
                                factory->microsecond_string()) ||
                 String::Equals(isolate, property,
                                factory->nanosecond_string()) ||
                 String::Equals(isolate, property, factory->eraYear_string())) {
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, value, ToIntegerThrowOnInfinity(isolate, value), JSObject);
      } else if (String::Equals(isolate, property,
                                factory->monthCode_string()) ||
                 String::Equals(isolate, property, factory->offset_string()) ||
                 String::Equals(isolate, property, factory->era_string())) {
        ASSIGN_RETURN_ON_EXCEPTION(isolate, value,
                                   Object::ToString(isolate, value), JSObject);
      }
    }

    // d. Perform ! CreateDataPropertyOrThrow(result, property, value).
    CHECK(JSReceiver::CreateDataProperty(isolate, result, property, value,
                                         Just(kThrowOnError))
              .FromJust());
  }

  // Only for PreparePartialTemporalFields
  if (partial) {
    // 5. If any is false, then
    if (!any) {
      // a. Throw a TypeError exception.
      THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_TYPE_ERROR(), JSObject);
    }
  }
  // 4. Return result.
  return result;
}

// #sec-temporal-preparetemporalfields
V8_WARN_UNUSED_RESULT MaybeHandle<JSObject> PrepareTemporalFields(
    Isolate* isolate, Handle<JSReceiver> fields, Handle<FixedArray> field_names,
    RequiredFields required) {
  TEMPORAL_ENTER_FUNC();

  return PrepareTemporalFieldsOrPartial(isolate, fields, field_names, required,
                                        false);
}

// Template for DateFromFields, YearMonthFromFields, and MonthDayFromFields
template <typename T>
MaybeHandle<T> FromFields(Isolate* isolate, Handle<JSReceiver> calendar,
                          Handle<JSReceiver> fields, Handle<Object> options,
                          Handle<String> property, InstanceType type) {
  Handle<Object> function;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, function, Object::GetProperty(isolate, calendar, property), T);
  if (!function->IsCallable()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kCalledNonCallable, property),
                    T);
  }
  Handle<Object> argv[] = {fields, options};
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result, Execution::Call(isolate, function, calendar, 2, argv),
      T);
  if ((!result->IsHeapObject()) ||
      HeapObject::cast(*result).map().instance_type() != type) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_TYPE_ERROR(), T);
  }
  return Handle<T>::cast(result);
}

// #sec-temporal-datefromfields
MaybeHandle<JSTemporalPlainDate> DateFromFields(Isolate* isolate,
                                                Handle<JSReceiver> calendar,
                                                Handle<JSReceiver> fields,
                                                Handle<Object> options) {
  return FromFields<JSTemporalPlainDate>(
      isolate, calendar, fields, options,
      isolate->factory()->dateFromFields_string(), JS_TEMPORAL_PLAIN_DATE_TYPE);
}

// #sec-temporal-yearmonthfromfields
MaybeHandle<JSTemporalPlainYearMonth> YearMonthFromFields(
    Isolate* isolate, Handle<JSReceiver> calendar, Handle<JSReceiver> fields,
    Handle<Object> options) {
  return FromFields<JSTemporalPlainYearMonth>(
      isolate, calendar, fields, options,
      isolate->factory()->yearMonthFromFields_string(),
      JS_TEMPORAL_PLAIN_YEAR_MONTH_TYPE);
}

// #sec-temporal-monthdayfromfields
MaybeHandle<JSTemporalPlainMonthDay> MonthDayFromFields(
    Isolate* isolate, Handle<JSReceiver> calendar, Handle<JSReceiver> fields,
    Handle<Object> options) {
  return FromFields<JSTemporalPlainMonthDay>(
      isolate, calendar, fields, options,
      isolate->factory()->monthDayFromFields_string(),
      JS_TEMPORAL_PLAIN_MONTH_DAY_TYPE);
}

// #sec-temporal-totemporaloverflow
Maybe<ShowOverflow> ToTemporalOverflow(Isolate* isolate, Handle<Object> options,
                                       const char* method_name) {
  // 1. If options is undefined, return "constrain".
  if (options->IsUndefined()) {
    return Just(ShowOverflow::kConstrain);
  }
  DCHECK(options->IsJSReceiver());
  // 2. Return ? GetOption(options, "overflow", « String », « "constrain",
  // "reject" », "constrain").
  return GetStringOption<ShowOverflow>(
      isolate, Handle<JSReceiver>::cast(options), "overflow", method_name,
      {"constrain", "reject"},
      {ShowOverflow::kConstrain, ShowOverflow::kReject},
      ShowOverflow::kConstrain);
}

// #sec-temporal-totemporaldisambiguation
Maybe<Disambiguation> ToTemporalDisambiguation(Isolate* isolate,
                                               Handle<JSReceiver> options,
                                               const char* method_name) {
  return GetStringOption<Disambiguation>(
      isolate, options, "disambiguation", method_name,
      {"compatible", "earlier", "later", "reject"},
      {Disambiguation::kCompatible, Disambiguation::kEarlier,
       Disambiguation::kLater, Disambiguation::kReject},
      Disambiguation::kCompatible);
}

// #sec-temporal-builtintimezonegetinstantfor
MaybeHandle<JSTemporalInstant> BuiltinTimeZoneGetInstantFor(
    Isolate* isolate, Handle<JSReceiver> time_zone,
    Handle<JSTemporalPlainDateTime> date_time, Disambiguation disambiguation,
    const char* method_name) {
  TEMPORAL_ENTER_FUNC();
  // 1. Assert: dateTime has an [[InitializedTemporalDateTime]] internal slot.
  // 2. Let possibleInstants be ? GetPossibleInstantsFor(timeZone, dateTime).
  Handle<FixedArray> possible_instants;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, possible_instants,
      GetPossibleInstantsFor(isolate, time_zone, date_time), JSTemporalInstant);
  // 3. Return ? DisambiguatePossibleInstants(possibleInstants, timeZone,
  // dateTime, disambiguation).
  return DisambiguatePossibleInstants(isolate, possible_instants, time_zone,
                                      date_time, disambiguation, method_name);
}

// #sec-temporal-totemporalinstant
MaybeHandle<JSTemporalInstant> ToTemporalInstant(Isolate* isolate,
                                                 Handle<Object> item,
                                                 const char* method) {
  TEMPORAL_ENTER_FUNC();

  // 1. If Type(item) is Object, then
  // a. If item has an [[InitializedTemporalInstant]] internal slot, then
  if (item->IsJSTemporalInstant()) {
    // i. Return item.
    return Handle<JSTemporalInstant>::cast(item);
  }
  // b. If item has an [[InitializedTemporalZonedDateTime]] internal slot, then
  if (item->IsJSTemporalZonedDateTime()) {
    // i. Return ! CreateTemporalInstant(item.[[Nanoseconds]]).
    Handle<BigInt> nanoseconds =
        handle(JSTemporalZonedDateTime::cast(*item).nanoseconds(), isolate);
    return temporal::CreateTemporalInstant(isolate, nanoseconds);
  }
  // 2. Let string be ? ToString(item).
  Handle<String> string;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, string, Object::ToString(isolate, item),
                             JSTemporalInstant);

  // 3. Let epochNanoseconds be ? ParseTemporalInstant(string).
  Handle<BigInt> epoch_nanoseconds;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, epoch_nanoseconds,
                             ParseTemporalInstant(isolate, string),
                             JSTemporalInstant);

  // 4. Return ? CreateTemporalInstant(ℤ(epochNanoseconds)).
  return temporal::CreateTemporalInstant(isolate, epoch_nanoseconds);
}

}  // namespace

namespace temporal {
// #sec-temporal-totemporalcalendar
MaybeHandle<JSReceiver> ToTemporalCalendar(
    Isolate* isolate, Handle<Object> temporal_calendar_like,
    const char* method_name) {
  Factory* factory = isolate->factory();
  // 1.If Type(temporalCalendarLike) is Object, then
  if (temporal_calendar_like->IsJSReceiver()) {
    // a. If temporalCalendarLike has an [[InitializedTemporalDate]],
    // [[InitializedTemporalDateTime]], [[InitializedTemporalMonthDay]],
    // [[InitializedTemporalTime]], [[InitializedTemporalYearMonth]], or
    // [[InitializedTemporalZonedDateTime]] internal slot, then i. Return
    // temporalCalendarLike.[[Calendar]].

#define EXTRACT_CALENDAR(T, obj)                                          \
  if (obj->IsJSTemporal##T()) {                                           \
    return handle(Handle<JSTemporal##T>::cast(obj)->calendar(), isolate); \
  }

    EXTRACT_CALENDAR(PlainDate, temporal_calendar_like)
    EXTRACT_CALENDAR(PlainDateTime, temporal_calendar_like)
    EXTRACT_CALENDAR(PlainMonthDay, temporal_calendar_like)
    EXTRACT_CALENDAR(PlainTime, temporal_calendar_like)
    EXTRACT_CALENDAR(PlainYearMonth, temporal_calendar_like)
    EXTRACT_CALENDAR(ZonedDateTime, temporal_calendar_like)

#undef EXTRACT_CALENDAR
    Handle<JSReceiver> obj = Handle<JSReceiver>::cast(temporal_calendar_like);

    // b. If ? HasProperty(temporalCalendarLike, "calendar") is false, return
    // temporalCalendarLike.
    bool has;
    MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, has,
        JSReceiver::HasProperty(isolate, obj, factory->calendar_string()),
        Handle<JSReceiver>());
    if (!has) {
      return obj;
    }
    // c.  Set temporalCalendarLike to ? Get(temporalCalendarLike, "calendar").
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, temporal_calendar_like,
        JSReceiver::GetProperty(isolate, obj, factory->calendar_string()),
        JSReceiver);
    // d. If Type(temporalCalendarLike) is Object
    if (temporal_calendar_like->IsJSReceiver()) {
      obj = Handle<JSReceiver>::cast(temporal_calendar_like);
      // and ? HasProperty(temporalCalendarLike, "calendar") is false,
      MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, has,
          JSReceiver::HasProperty(isolate, obj, factory->calendar_string()),
          Handle<JSReceiver>());
      if (!has) {
        // return temporalCalendarLike.
        return obj;
      }
    }
  }

  // 2. Let identifier be ? ToString(temporalCalendarLike).
  Handle<String> identifier;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, identifier,
                             Object::ToString(isolate, temporal_calendar_like),
                             JSReceiver);
  // 3. If ! IsBuiltinCalendar(identifier) is false, then
  if (!IsBuiltinCalendar(isolate, identifier)) {
    // a. Let identifier be ? ParseTemporalCalendarString(identifier).
    ASSIGN_RETURN_ON_EXCEPTION(isolate, identifier,
                               ParseTemporalCalendarString(isolate, identifier),
                               JSReceiver);
  }
  // 4. Return ? CreateTemporalCalendar(identifier).
  return CreateTemporalCalendar(isolate, identifier);
}

}  // namespace temporal

namespace {
// #sec-temporal-totemporalcalendarwithisodefault
MaybeHandle<JSReceiver> ToTemporalCalendarWithISODefault(
    Isolate* isolate, Handle<Object> temporal_calendar_like,
    const char* method_name) {
  TEMPORAL_ENTER_FUNC();

  // 1. If temporalCalendarLike is undefined, then
  if (temporal_calendar_like->IsUndefined()) {
    // a. Return ? GetISO8601Calendar().
    return temporal::GetISO8601Calendar(isolate);
  }
  // 2. Return ? ToTemporalCalendar(temporalCalendarLike).
  return temporal::ToTemporalCalendar(isolate, temporal_calendar_like,
                                      method_name);
}

// #sec-temporal-totemporaldate
MaybeHandle<JSTemporalPlainDate> ToTemporalDate(Isolate* isolate,
                                                Handle<Object> item_obj,
                                                Handle<Object> options,
                                                const char* method_name) {
  TEMPORAL_ENTER_FUNC();

  Factory* factory = isolate->factory();
  // 2. Assert: Type(options) is Object or Undefined.
  DCHECK(options->IsJSReceiver() || options->IsUndefined());
  // 3. If Type(item) is Object, then
  if (item_obj->IsJSReceiver()) {
    Handle<JSReceiver> item = Handle<JSReceiver>::cast(item_obj);
    // a. If item has an [[InitializedTemporalDate]] internal slot, then
    // i. Return item.
    if (item->IsJSTemporalPlainDate()) {
      return Handle<JSTemporalPlainDate>::cast(item);
    }
    // b. If item has an [[InitializedTemporalZonedDateTime]] internal slot,
    // then
    if (item->IsJSTemporalZonedDateTime()) {
      // i. Let instant be ! CreateTemporalInstant(item.[[Nanoseconds]]).
      Handle<JSTemporalZonedDateTime> zoned_date_time =
          Handle<JSTemporalZonedDateTime>::cast(item);
      Handle<JSTemporalInstant> instant;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, instant,
          temporal::CreateTemporalInstant(
              isolate, Handle<BigInt>(zoned_date_time->nanoseconds(), isolate)),
          JSTemporalPlainDate);
      // ii. Let plainDateTime be ?
      // BuiltinTimeZoneGetPlainDateTimeFor(item.[[TimeZone]],
      // instant, item.[[Calendar]]).
      Handle<JSTemporalPlainDateTime> plain_date_time;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, plain_date_time,
          temporal::BuiltinTimeZoneGetPlainDateTimeFor(
              isolate,
              Handle<JSReceiver>(zoned_date_time->time_zone(), isolate),
              instant, Handle<JSReceiver>(zoned_date_time->calendar(), isolate),
              method_name),
          JSTemporalPlainDate);
      // iii. Return ! CreateTemporalDate(plainDateTime.[[ISOYear]],
      // plainDateTime.[[ISOMonth]], plainDateTime.[[ISODay]],
      // plainDateTime.[[Calendar]]).
      return CreateTemporalDate(
          isolate,
          {plain_date_time->iso_year(), plain_date_time->iso_month(),
           plain_date_time->iso_day()},
          Handle<JSReceiver>(plain_date_time->calendar(), isolate));
    }

    // c. If item has an [[InitializedTemporalDateTime]] internal slot, then
    // i. Return ! CreateTemporalDate(item.[[ISOYear]], item.[[ISOMonth]],
    // item.[[ISODay]], item.[[Calendar]]).
    if (item->IsJSTemporalPlainDateTime()) {
      Handle<JSTemporalPlainDateTime> date_time =
          Handle<JSTemporalPlainDateTime>::cast(item);
      return CreateTemporalDate(
          isolate,
          {date_time->iso_year(), date_time->iso_month(), date_time->iso_day()},
          handle(date_time->calendar(), isolate));
    }

    // d. Let calendar be ? GetTemporalCalendarWithISODefault(item).
    Handle<JSReceiver> calendar;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, calendar,
        GetTemporalCalendarWithISODefault(isolate, item, method_name),
        JSTemporalPlainDate);
    // e. Let fieldNames be ? CalendarFields(calendar, « "day", "month",
    // "monthCode", "year" »).
    Handle<FixedArray> field_names = factory->NewFixedArray(4);
    field_names->set(0, ReadOnlyRoots(isolate).day_string());
    field_names->set(1, ReadOnlyRoots(isolate).month_string());
    field_names->set(2, ReadOnlyRoots(isolate).monthCode_string());
    field_names->set(3, ReadOnlyRoots(isolate).year_string());
    ASSIGN_RETURN_ON_EXCEPTION(isolate, field_names,
                               CalendarFields(isolate, calendar, field_names),
                               JSTemporalPlainDate);
    // f. Let fields be ? PrepareTemporalFields(item,
    // fieldNames, «»).
    Handle<JSReceiver> fields;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, fields,
                               PrepareTemporalFields(isolate, item, field_names,
                                                     RequiredFields::kNone),
                               JSTemporalPlainDate);
    // g. Return ? DateFromFields(calendar, fields, options).
    return DateFromFields(isolate, calendar, fields, options);
  }
  // 4. Perform ? ToTemporalOverflow(options).
  ShowOverflow overflow;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, overflow, ToTemporalOverflow(isolate, options, method_name),
      Handle<JSTemporalPlainDate>());
  USE(overflow);

  // 5. Let string be ? ToString(item).
  Handle<String> string;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, string,
                             Object::ToString(isolate, item_obj),
                             JSTemporalPlainDate);
  // 6. Let result be ? ParseTemporalDateString(string).
  DateRecord result;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, result, ParseTemporalDateString(isolate, string),
      Handle<JSTemporalPlainDate>());

  // 7. Assert: ! IsValidISODate(result.[[Year]], result.[[Month]],
  // result.[[Day]]) is true.
  DCHECK(IsValidISODate(isolate, result.date));
  // 8. Let calendar be ? ToTemporalCalendarWithISODefault(result.[[Calendar]]).
  Handle<Object> calendar_string;
  if (result.calendar->length() == 0) {
    calendar_string = factory->undefined_value();
  } else {
    calendar_string = result.calendar;
  }
  Handle<JSReceiver> calendar;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, calendar,
      ToTemporalCalendarWithISODefault(isolate, calendar_string, method_name),
      JSTemporalPlainDate);
  // 9. Return ? CreateTemporalDate(result.[[Year]], result.[[Month]],
  // result.[[Day]], calendar).
  return CreateTemporalDate(isolate, result.date, calendar);
}

MaybeHandle<JSTemporalPlainDate> ToTemporalDate(Isolate* isolate,
                                                Handle<Object> item_obj,
                                                const char* method_name) {
  // 1. If options is not present, set options to undefined.
  return ToTemporalDate(isolate, item_obj,
                        isolate->factory()->undefined_value(), method_name);
}

// #sec-isintegralnumber
bool IsIntegralNumber(Isolate* isolate, Handle<Object> argument) {
  // 1. If Type(argument) is not Number, return false.
  if (!argument->IsNumber()) return false;
  // 2. If argument is NaN, +∞𝔽, or -∞𝔽, return false.
  double number = argument->Number();
  if (!std::isfinite(number)) return false;
  // 3. If floor(abs(ℝ(argument))) ≠ abs(ℝ(argument)), return false.
  if (std::floor(std::abs(number)) != std::abs(number)) return false;
  // 4. Return true.
  return true;
}

// #sec-temporal-tointegerwithoutrounding
Maybe<double> ToIntegerWithoutRounding(Isolate* isolate,
                                       Handle<Object> argument) {
  // 1. Let number be ? ToNumber(argument).
  Handle<Object> number;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, number, Object::ToNumber(isolate, argument), Nothing<double>());
  // 2. If number is NaN, +0𝔽, or −0𝔽 return 0.
  if (number->IsNaN() || number->Number() == 0) {
    return Just(static_cast<double>(0));
  }
  // 3. If IsIntegralNumber(number) is false, throw a RangeError exception.
  if (!IsIntegralNumber(isolate, number)) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(), Nothing<double>());
  }
  // 4. Return ℝ(number).
  return Just(number->Number());
}

}  // namespace

namespace temporal {

// #sec-temporal-regulatetime
Maybe<TimeRecord> RegulateTime(Isolate* isolate, const TimeRecord& time,
                               ShowOverflow overflow) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: hour, minute, second, millisecond, microsecond and nanosecond
  // are integers.
  // 2. Assert: overflow is either "constrain" or "reject".
  switch (overflow) {
    case ShowOverflow::kConstrain: {
      TimeRecord result(time);
      // 3. If overflow is "constrain", then
      // a. Return ! ConstrainTime(hour, minute, second, millisecond,
      // microsecond, nanosecond).
      result.time.hour = std::max(std::min(result.time.hour, 23), 0);
      result.time.minute = std::max(std::min(result.time.minute, 59), 0);
      result.time.second = std::max(std::min(result.time.second, 59), 0);
      result.time.millisecond =
          std::max(std::min(result.time.millisecond, 999), 0);
      result.time.microsecond =
          std::max(std::min(result.time.microsecond, 999), 0);
      result.time.nanosecond =
          std::max(std::min(result.time.nanosecond, 999), 0);
      return Just(result);
    }
    case ShowOverflow::kReject:
      // 4. If overflow is "reject", then
      // a. If ! IsValidTime(hour, minute, second, millisecond, microsecond,
      // nanosecond) is false, throw a RangeError exception.
      if (!IsValidTime(isolate, time.time)) {
        THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                     NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                                     Nothing<TimeRecord>());
      }
      // b. Return the new Record { [[Hour]]: hour, [[Minute]]: minute,
      // [[Second]]: second, [[Millisecond]]: millisecond, [[Microsecond]]:
      // microsecond, [[Nanosecond]]: nanosecond }.
      return Just(time);
  }
}

// #sec-temporal-totemporaltime
MaybeHandle<JSTemporalPlainTime> ToTemporalTime(Isolate* isolate,
                                                Handle<Object> item_obj,
                                                ShowOverflow overflow,
                                                const char* method_name) {
  Factory* factory = isolate->factory();
  TimeRecord result;
  // 2. Assert: overflow is either "constrain" or "reject".
  // 3. If Type(item) is Object, then
  if (item_obj->IsJSReceiver()) {
    Handle<JSReceiver> item = Handle<JSReceiver>::cast(item_obj);
    // a. If item has an [[InitializedTemporalTime]] internal slot, then
    // i. Return item.
    if (item->IsJSTemporalPlainTime()) {
      return Handle<JSTemporalPlainTime>::cast(item);
    }
    // b. If item has an [[InitializedTemporalZonedDateTime]] internal slot,
    // then
    if (item->IsJSTemporalZonedDateTime()) {
      // i. Let instant be ! CreateTemporalInstant(item.[[Nanoseconds]]).
      Handle<JSTemporalZonedDateTime> zoned_date_time =
          Handle<JSTemporalZonedDateTime>::cast(item);
      Handle<JSTemporalInstant> instant;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, instant,
          CreateTemporalInstant(
              isolate, Handle<BigInt>(zoned_date_time->nanoseconds(), isolate)),
          JSTemporalPlainTime);
      // ii. Set plainDateTime to ?
      // BuiltinTimeZoneGetPlainDateTimeFor(item.[[TimeZone]],
      // instant, item.[[Calendar]]).
      Handle<JSTemporalPlainDateTime> plain_date_time;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, plain_date_time,
          BuiltinTimeZoneGetPlainDateTimeFor(
              isolate,
              Handle<JSReceiver>(zoned_date_time->time_zone(), isolate),
              instant, Handle<JSReceiver>(zoned_date_time->calendar(), isolate),
              method_name),
          JSTemporalPlainTime);
      // iii. Return !
      // CreateTemporalTime(plainDateTime.[[ISOHour]],
      // plainDateTime.[[ISOMinute]], plainDateTime.[[ISOSecond]],
      // plainDateTime.[[ISOMillisecond]], plainDateTime.[[ISOMicrosecond]],
      // plainDateTime.[[ISONanosecond]]).
      return CreateTemporalTime(
          isolate,
          {plain_date_time->iso_hour(), plain_date_time->iso_minute(),
           plain_date_time->iso_second(), plain_date_time->iso_millisecond(),
           plain_date_time->iso_microsecond(),
           plain_date_time->iso_nanosecond()});
    }
    // c. If item has an [[InitializedTemporalDateTime]] internal slot, then
    if (item->IsJSTemporalPlainDateTime()) {
      // i. Return ! CreateTemporalTime(item.[[ISOHour]], item.[[ISOMinute]],
      // item.[[ISOSecond]], item.[[ISOMillisecond]], item.[[ISOMicrosecond]],
      // item.[[ISONanosecond]]).
      Handle<JSTemporalPlainDateTime> date_time =
          Handle<JSTemporalPlainDateTime>::cast(item);
      return CreateTemporalTime(
          isolate, {date_time->iso_hour(), date_time->iso_minute(),
                    date_time->iso_second(), date_time->iso_millisecond(),
                    date_time->iso_microsecond(), date_time->iso_nanosecond()});
    }
    // d. Let calendar be ? GetTemporalCalendarWithISODefault(item).
    Handle<JSReceiver> calendar;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, calendar,
        GetTemporalCalendarWithISODefault(isolate, item, method_name),
        JSTemporalPlainTime);
    // e. If ? ToString(calendar) is not "iso8601", then
    Handle<String> identifier;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, identifier,
                               Object::ToString(isolate, calendar),
                               JSTemporalPlainTime);
    if (!String::Equals(isolate, factory->iso8601_string(), identifier)) {
      // i. Throw a RangeError exception.
      THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                      JSTemporalPlainTime);
    }
    // f. Let result be ? ToTemporalTimeRecord(item).
    MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, result, ToTemporalTimeRecord(isolate, item, method_name),
        Handle<JSTemporalPlainTime>());
    // g. Set result to ? RegulateTime(result.[[Hour]], result.[[Minute]],
    // result.[[Second]], result.[[Millisecond]], result.[[Microsecond]],
    // result.[[Nanosecond]], overflow).
    MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, result, RegulateTime(isolate, result, overflow),
        Handle<JSTemporalPlainTime>());
  } else {
    // 4. Else,
    // a. Let string be ? ToString(item).
    Handle<String> string;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, string,
                               Object::ToString(isolate, item_obj),
                               JSTemporalPlainTime);
    // b. Let result be ? ParseTemporalTimeString(string).
    MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, result, ParseTemporalTimeString(isolate, string),
        Handle<JSTemporalPlainTime>());
    // c. Assert: ! IsValidTime(result.[[Hour]], result.[[Minute]],
    // result.[[Second]], result.[[Millisecond]], result.[[Microsecond]],
    // result.[[Nanosecond]]) is true.
    DCHECK(IsValidTime(isolate, result.time));
    // d. If result.[[Calendar]] is not one of undefined or "iso8601", then
    if ((result.calendar->length() > 0) /* not undefined */ &&
        !String::Equals(isolate, result.calendar,
                        isolate->factory()->iso8601_string())) {
      // i. Throw a RangeError exception.
      THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                      JSTemporalPlainTime);
    }
  }
  // 5. Return ? CreateTemporalTime(result.[[Hour]], result.[[Minute]],
  // result.[[Second]], result.[[Millisecond]], result.[[Microsecond]],
  // result.[[Nanosecond]]).
  return CreateTemporalTime(isolate, result.time);
}

// #sec-temporal-totemporaldurationrecord
Maybe<DurationRecord> ToTemporalDurationRecord(
    Isolate* isolate, Handle<Object> temporal_duration_like,
    const char* method_name) {
  TEMPORAL_ENTER_FUNC();

  // 1. If Type(temporalDurationLike) is not Object, then
  if (!temporal_duration_like->IsJSReceiver()) {
    // a. Let string be ? ToString(temporalDurationLike).
    Handle<String> string;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, string, Object::ToString(isolate, temporal_duration_like),
        Nothing<DurationRecord>());
    // b. Let result be ? ParseTemporalDurationString(string).
    return ParseTemporalDurationString(isolate, string);
  }
  // 2. If temporalDurationLike has an [[InitializedTemporalDuration]] internal
  // slot, then
  if (temporal_duration_like->IsJSTemporalDuration()) {
    // a. Return ! CreateDurationRecord(temporalDurationLike.[[Years]],
    // temporalDurationLike.[[Months]], temporalDurationLike.[[Weeks]],
    // temporalDurationLike.[[Days]], temporalDurationLike.[[Hours]],
    // temporalDurationLike.[[Minutes]], temporalDurationLike.[[Seconds]],
    // temporalDurationLike.[[Milliseconds]],
    // temporalDurationLike.[[Microseconds]],
    // temporalDurationLike.[[Nanoseconds]]).
    Handle<JSTemporalDuration> duration =
        Handle<JSTemporalDuration>::cast(temporal_duration_like);
    return DurationRecord::Create(
        isolate, duration->years().Number(), duration->months().Number(),
        duration->weeks().Number(), duration->days().Number(),
        duration->hours().Number(), duration->minutes().Number(),
        duration->seconds().Number(), duration->milliseconds().Number(),
        duration->microseconds().Number(), duration->nanoseconds().Number());
  }
  // 3. Let result be a new Record with all the internal slots given in the
  // Internal Slot column in Table 7.
  // 4. Let any be false.
  bool any = false;

  // 5. For each row of Table 7, except the header row, in table order, do
  DurationRecord record;
  Factory* factory = isolate->factory();
  std::array<std::pair<Handle<String>, double*>, 10> table7 = {
      {{factory->days_string(), &record.time_duration.days},
       {factory->hours_string(), &record.time_duration.hours},
       {factory->microseconds_string(), &record.time_duration.microseconds},
       {factory->milliseconds_string(), &record.time_duration.milliseconds},
       {factory->minutes_string(), &record.time_duration.minutes},
       {factory->months_string(), &record.months},
       {factory->nanoseconds_string(), &record.time_duration.nanoseconds},
       {factory->seconds_string(), &record.time_duration.seconds},
       {factory->weeks_string(), &record.weeks},
       {factory->years_string(), &record.years}}};

  for (const auto& row : table7) {
    Handle<Object> val;
    // a. Let prop be the Property value of the current row.
    // b. Let val be ? Get(temporalDurationLike, prop).
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, val,
        Object::GetPropertyOrElement(isolate, temporal_duration_like,
                                     row.first),
        Nothing<DurationRecord>());
    // c. If val is undefined, then
    if (val->IsUndefined()) {
      // i. Set result's internal slot whose name is the Internal Slot
      // value of the current row to 0.
      *(row.second) = 0;
      // d. Else,
    } else {
      // i. Set any to true.
      any = true;
      // ii. Let val be 𝔽(? ToIntegerWithoutRounding(val)).
      MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, *(row.second), ToIntegerWithoutRounding(isolate, val),
          Nothing<DurationRecord>());
      // iii. Set result's field whose name is the Field Name value of the
      // current row to val.
    }
  }

  // 6. If any is false, then
  if (!any) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR_RETURN_VALUE(isolate, NEW_TEMPORAL_INVALID_ARG_TYPE_ERROR(),
                                 Nothing<DurationRecord>());
  }
  // 7. If ! IsValidDuration(result.[[Years]], result.[[Months]],
  // result.[[Weeks]] result.[[Days]], result.[[Hours]], result.[[Minutes]],
  // result.[[Seconds]], result.[[Milliseconds]], result.[[Microseconds]],
  // result.[[Nanoseconds]]) is false, then
  if (!IsValidDuration(isolate, record)) {
    // a. Throw a RangeError exception.
    THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                 NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                                 Nothing<DurationRecord>());
  }
  // 8. Return result.
  return Just(record);
}

// #sec-temporal-totemporalduration
MaybeHandle<JSTemporalDuration> ToTemporalDuration(Isolate* isolate,
                                                   Handle<Object> item,
                                                   const char* method_name) {
  TEMPORAL_ENTER_FUNC();

  DurationRecord result;
  // 1. If Type(item) is Object and item has an [[InitializedTemporalDuration]]
  // internal slot, then
  if (item->IsJSTemporalDuration()) {
    // a. Return item.
    return Handle<JSTemporalDuration>::cast(item);
  }
  // 2. Let result be ? ToTemporalDurationRecord(item).
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, result, ToTemporalDurationRecord(isolate, item, method_name),
      Handle<JSTemporalDuration>());

  // 3. Return ? CreateTemporalDuration(result.[[Years]], result.[[Months]],
  // result.[[Weeks]], result.[[Days]], result.[[Hours]], result.[[Minutes]],
  // result.[[Seconds]], result.[[Milliseconds]], result.[[Microseconds]],
  // result.[[Nanoseconds]]).
  return CreateTemporalDuration(isolate, result);
}

// #sec-temporal-totemporaltimezone
MaybeHandle<JSReceiver> ToTemporalTimeZone(
    Isolate* isolate, Handle<Object> temporal_time_zone_like,
    const char* method_name) {
  TEMPORAL_ENTER_FUNC();

  Factory* factory = isolate->factory();
  // 1. If Type(temporalTimeZoneLike) is Object, then
  if (temporal_time_zone_like->IsJSReceiver()) {
    // a. If temporalTimeZoneLike has an [[InitializedTemporalZonedDateTime]]
    // internal slot, then
    if (temporal_time_zone_like->IsJSTemporalZonedDateTime()) {
      // i. Return temporalTimeZoneLike.[[TimeZone]].
      Handle<JSTemporalZonedDateTime> zoned_date_time =
          Handle<JSTemporalZonedDateTime>::cast(temporal_time_zone_like);
      return handle(zoned_date_time->time_zone(), isolate);
    }
    Handle<JSReceiver> obj = Handle<JSReceiver>::cast(temporal_time_zone_like);
    // b. If ? HasProperty(temporalTimeZoneLike, "timeZone") is false,
    bool has;
    MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, has,
        JSReceiver::HasProperty(isolate, obj, factory->timeZone_string()),
        Handle<JSReceiver>());
    if (!has) {
      // return temporalTimeZoneLike.
      return obj;
    }
    // c. Set temporalTimeZoneLike to ?
    // Get(temporalTimeZoneLike, "timeZone").
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, temporal_time_zone_like,
        JSReceiver::GetProperty(isolate, obj, factory->timeZone_string()),
        JSReceiver);
    // d. If Type(temporalTimeZoneLike)
    if (temporal_time_zone_like->IsJSReceiver()) {
      // is Object and ? HasProperty(temporalTimeZoneLike, "timeZone") is false,
      obj = Handle<JSReceiver>::cast(temporal_time_zone_like);
      MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, has,
          JSReceiver::HasProperty(isolate, obj, factory->timeZone_string()),
          Handle<JSReceiver>());
      if (!has) {
        // return temporalTimeZoneLike.
        return obj;
      }
    }
  }
  Handle<String> identifier;
  // 2. Let identifier be ? ToString(temporalTimeZoneLike).
  ASSIGN_RETURN_ON_EXCEPTION(isolate, identifier,
                             Object::ToString(isolate, temporal_time_zone_like),
                             JSReceiver);
  // 3. Let result be ? ParseTemporalTimeZone(identifier).
  Handle<String> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result, ParseTemporalTimeZone(isolate, identifier), JSReceiver);

  // 4. Return ? CreateTemporalTimeZone(result).
  return temporal::CreateTemporalTimeZone(isolate, result);
}

}  // namespace temporal

namespace {
// #sec-temporal-systemdatetime
MaybeHandle<JSTemporalPlainDateTime> SystemDateTime(
    Isolate* isolate, Handle<Object> temporal_time_zone_like,
    Handle<Object> calendar_like, const char* method_name) {
  TEMPORAL_ENTER_FUNC();

  Handle<JSReceiver> time_zone;
  // 1. 1. If temporalTimeZoneLike is undefined, then
  if (temporal_time_zone_like->IsUndefined()) {
    // a. Let timeZone be ! SystemTimeZone().
    ASSIGN_RETURN_ON_EXCEPTION(isolate, time_zone, SystemTimeZone(isolate),
                               JSTemporalPlainDateTime);
  } else {
    // 2. Else,
    // a. Let timeZone be ? ToTemporalTimeZone(temporalTimeZoneLike).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, time_zone,
        temporal::ToTemporalTimeZone(isolate, temporal_time_zone_like,
                                     method_name),
        JSTemporalPlainDateTime);
  }
  Handle<JSReceiver> calendar;
  // 3. Let calendar be ? ToTemporalCalendar(calendarLike).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, calendar,
      temporal::ToTemporalCalendar(isolate, calendar_like, method_name),
      JSTemporalPlainDateTime);
  // 4. Let instant be ! SystemInstant().
  Handle<JSTemporalInstant> instant;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, instant, SystemInstant(isolate),
                             JSTemporalPlainDateTime);
  // 5. Return ? BuiltinTimeZoneGetPlainDateTimeFor(timeZone, instant,
  // calendar).
  return temporal::BuiltinTimeZoneGetPlainDateTimeFor(
      isolate, time_zone, instant, calendar, method_name);
}

MaybeHandle<JSTemporalZonedDateTime> SystemZonedDateTime(
    Isolate* isolate, Handle<Object> temporal_time_zone_like,
    Handle<Object> calendar_like, const char* method_name) {
  TEMPORAL_ENTER_FUNC();

  Handle<JSReceiver> time_zone;
  // 1. 1. If temporalTimeZoneLike is undefined, then
  if (temporal_time_zone_like->IsUndefined()) {
    // a. Let timeZone be ! SystemTimeZone().
    ASSIGN_RETURN_ON_EXCEPTION(isolate, time_zone, SystemTimeZone(isolate),
                               JSTemporalZonedDateTime);
  } else {
    // 2. Else,
    // a. Let timeZone be ? ToTemporalTimeZone(temporalTimeZoneLike).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, time_zone,
        temporal::ToTemporalTimeZone(isolate, temporal_time_zone_like,
                                     method_name),
        JSTemporalZonedDateTime);
  }
  Handle<JSReceiver> calendar;
  // 3. Let calendar be ? ToTemporalCalendar(calendarLike).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, calendar,
      temporal::ToTemporalCalendar(isolate, calendar_like, method_name),
      JSTemporalZonedDateTime);
  // 4. Let ns be ! SystemUTCEpochNanoseconds().
  Handle<BigInt> ns;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, ns, SystemUTCEpochNanoseconds(isolate),
                             JSTemporalZonedDateTime);
  // Return ? CreateTemporalZonedDateTime(ns, timeZone, calendar).
  return CreateTemporalZonedDateTime(isolate, ns, time_zone, calendar);
}

#define COMPARE_RESULT_TO_SIGN(r)  \
  ((r) == ComparisonResult::kEqual \
       ? 0                         \
       : ((r) == ComparisonResult::kLessThan ? -1 : 1))

// #sec-temporal-formattimezoneoffsetstring
MaybeHandle<String> FormatTimeZoneOffsetString(Isolate* isolate,
                                               int64_t offset_nanoseconds) {
  IncrementalStringBuilder builder(isolate);
  // 1. Assert: offsetNanoseconds is an integer.
  // 2. If offsetNanoseconds ≥ 0, let sign be "+"; otherwise, let sign be "-".
  builder.AppendCString((offset_nanoseconds >= 0) ? "+" : "-");
  // 3. Let offsetNanoseconds be abs(offsetNanoseconds).
  offset_nanoseconds = std::abs(offset_nanoseconds);
  // 3. Let nanoseconds be offsetNanoseconds modulo 10^9.
  int64_t nanoseconds = offset_nanoseconds % 1000000000;
  // 4. Let seconds be floor(offsetNanoseconds / 10^9) modulo 60.
  int64_t seconds = (offset_nanoseconds / 1000000000) % 60;
  // 5. Let minutes be floor(offsetNanoseconds / (6 × 10^10)) modulo 60.
  int64_t minutes = (offset_nanoseconds / 60000000000) % 60;
  // 6. Let hours be floor(offsetNanoseconds / (3.6 × 10^12)).
  int64_t hours = offset_nanoseconds / 3600000000000;
  // 7. Let h be hours, formatted as a two-digit decimal number, padded to the
  // left with a zero if necessary.
  if (hours < 10) {
    builder.AppendCStringLiteral("0");
  }
  builder.AppendInt(static_cast<int32_t>(hours));
  // 8. Let m be minutes, formatted as a two-digit decimal number, padded to the
  // left with a zero if necessary.
  builder.AppendCString((minutes < 10) ? ":0" : ":");
  builder.AppendInt(static_cast<int>(minutes));
  // 9. Let s be seconds, formatted as a two-digit decimal number, padded to the
  // left with a zero if necessary.
  // 10. If nanoseconds ≠ 0, then
  if (nanoseconds != 0) {
    builder.AppendCString((seconds < 10) ? ":0" : ":");
    builder.AppendInt(static_cast<int>(seconds));
    builder.AppendCStringLiteral(".");
    // a. Let fraction be nanoseconds, formatted as a nine-digit decimal number,
    // padded to the left with zeroes if necessary.
    // b. Set fraction to the longest possible substring of fraction starting at
    // position 0 and not ending with the code unit 0x0030 (DIGIT ZERO).
    int64_t divisor = 100000000;
    do {
      builder.AppendInt(static_cast<int>(nanoseconds / divisor));
      nanoseconds %= divisor;
      divisor /= 10;
    } while (nanoseconds > 0);
    // c. Let post be the string-concatenation of the code unit 0x003A (COLON),
    // s, the code unit 0x002E (FULL STOP), and fraction.
    // 11. Else if seconds ≠ 0, then
  } else if (seconds != 0) {
    // a. Let post be the string-concatenation of the code unit 0x003A (COLON)
    // and s.
    builder.AppendCString((seconds < 10) ? ":0" : ":");
    builder.AppendInt(static_cast<int>(seconds));
  }
  // 12. Return the string-concatenation of sign, h, the code unit 0x003A
  // (COLON), m, and post.
  return builder.Finish();
}

// #sec-temporal-builtintimezonegetoffsetstringfor
MaybeHandle<String> BuiltinTimeZoneGetOffsetStringFor(
    Isolate* isolate, Handle<JSReceiver> time_zone,
    Handle<JSTemporalInstant> instant, const char* method_name) {
  TEMPORAL_ENTER_FUNC();
  // 1. Let offsetNanoseconds be ? GetOffsetNanosecondsFor(timeZone, instant).
  int64_t offset_nanoseconds;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, offset_nanoseconds,
      GetOffsetNanosecondsFor(isolate, time_zone, instant, method_name),
      Handle<String>());

  // 2. Return ! FormatTimeZoneOffsetString(offsetNanoseconds).
  return FormatTimeZoneOffsetString(isolate, offset_nanoseconds);
}

// #sec-temporal-parseisodatetime
Maybe<DateTimeRecord> ParseISODateTime(Isolate* isolate,
                                       Handle<String> iso_string,
                                       const ParsedISO8601Result& parsed) {
  TEMPORAL_ENTER_FUNC();

  DateTimeRecord result;
  // 5. Set year to ! ToIntegerOrInfinity(year).
  result.date.year = parsed.date_year;
  // 6. If month is undefined, then
  if (parsed.date_month_is_undefined()) {
    // a. Set month to 1.
    result.date.month = 1;
    // 7. Else,
  } else {
    // a. Set month to ! ToIntegerOrInfinity(month).
    result.date.month = parsed.date_month;
  }

  // 8. If day is undefined, then
  if (parsed.date_day_is_undefined()) {
    // a. Set day to 1.
    result.date.day = 1;
    // 9. Else,
  } else {
    // a. Set day to ! ToIntegerOrInfinity(day).
    result.date.day = parsed.date_day;
  }
  // 10. Set hour to ! ToIntegerOrInfinity(hour).
  result.time.hour = parsed.time_hour_is_undefined() ? 0 : parsed.time_hour;
  // 11. Set minute to ! ToIntegerOrInfinity(minute).
  result.time.minute =
      parsed.time_minute_is_undefined() ? 0 : parsed.time_minute;
  // 12. Set second to ! ToIntegerOrInfinity(second).
  result.time.second =
      parsed.time_second_is_undefined() ? 0 : parsed.time_second;
  // 13. If second is 60, then
  if (result.time.second == 60) {
    // a. Set second to 59.
    result.time.second = 59;
  }
  // 14. If fraction is not undefined, then
  if (!parsed.time_nanosecond_is_undefined()) {
    // a. Set fraction to the string-concatenation of the previous value of
    // fraction and the string "000000000".
    // b. Let millisecond be the String value equal to the substring of fraction
    // from 0 to 3. c. Set millisecond to ! ToIntegerOrInfinity(millisecond).
    result.time.millisecond = parsed.time_nanosecond / 1000000;
    // d. Let microsecond be the String value equal to the substring of fraction
    // from 3 to 6. e. Set microsecond to ! ToIntegerOrInfinity(microsecond).
    result.time.microsecond = (parsed.time_nanosecond / 1000) % 1000;
    // f. Let nanosecond be the String value equal to the substring of fraction
    // from 6 to 9. g. Set nanosecond to ! ToIntegerOrInfinity(nanosecond).
    result.time.nanosecond = (parsed.time_nanosecond % 1000);
    // 15. Else,
  } else {
    // a. Let millisecond be 0.
    result.time.millisecond = 0;
    // b. Let microsecond be 0.
    result.time.microsecond = 0;
    // c. Let nanosecond be 0.
    result.time.nanosecond = 0;
  }
  // 16. If ! IsValidISODate(year, month, day) is false, throw a RangeError
  // exception.
  if (!IsValidISODate(isolate, result.date)) {
    THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                 NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                                 Nothing<DateTimeRecord>());
  }
  // 17. If ! IsValidTime(hour, minute, second, millisecond, microsecond,
  // nanosecond) is false, throw a RangeError exception.
  if (!IsValidTime(isolate, result.time)) {
    THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                 NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                                 Nothing<DateTimeRecord>());
  }
  // 18. Return the Record { [[Year]]: year, [[Month]]: month, [[Day]]: day,
  // [[Hour]]: hour, [[Minute]]: minute, [[Second]]: second, [[Millisecond]]:
  // millisecond, [[Microsecond]]: microsecond, [[Nanosecond]]: nanosecond,
  // [[Calendar]]: calendar }.
  if (parsed.calendar_name_length == 0) {
    result.calendar = isolate->factory()->empty_string();
  } else {
    result.calendar = isolate->factory()->NewSubString(
        iso_string, parsed.calendar_name_start,
        parsed.calendar_name_start + parsed.calendar_name_length);
  }
  return Just(result);
}

// #sec-temporal-parsetemporaldatestring
Maybe<DateRecord> ParseTemporalDateString(Isolate* isolate,
                                          Handle<String> iso_string) {
  TEMPORAL_ENTER_FUNC();
  // 1. Assert: Type(isoString) is String.
  // 2. If isoString does not satisfy the syntax of a TemporalDateString
  // (see 13.33), then
  base::Optional<ParsedISO8601Result> parsed =
      TemporalParser::ParseTemporalDateString(isolate, iso_string);
  if (!parsed.has_value()) {
    // a. Throw a *RangeError* exception.
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(), Nothing<DateRecord>());
  }
  // 3. If _isoString_ contains a |UTCDesignator|, then
  if (parsed->utc_designator) {
    // a. Throw a *RangeError* exception.
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(), Nothing<DateRecord>());
  }
  // 3. Let result be ? ParseISODateTime(isoString).
  DateTimeRecord result;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, result, ParseISODateTime(isolate, iso_string, *parsed),
      Nothing<DateRecord>());
  // 4. Return the Record { [[Year]]: result.[[Year]], [[Month]]:
  // result.[[Month]], [[Day]]: result.[[Day]], [[Calendar]]:
  // result.[[Calendar]] }.
  DateRecord ret = {result.date, result.calendar};
  return Just(ret);
}

// #sec-temporal-parsetemporaltimestring
Maybe<TimeRecord> ParseTemporalTimeString(Isolate* isolate,
                                          Handle<String> iso_string) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: Type(isoString) is String.
  // 2. If isoString does not satisfy the syntax of a TemporalTimeString
  // (see 13.33), then
  base::Optional<ParsedISO8601Result> parsed =
      TemporalParser::ParseTemporalTimeString(isolate, iso_string);
  if (!parsed.has_value()) {
    // a. Throw a *RangeError* exception.
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(), Nothing<TimeRecord>());
  }

  // 3. If _isoString_ contains a |UTCDesignator|, then
  if (parsed->utc_designator) {
    // a. Throw a *RangeError* exception.
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(), Nothing<TimeRecord>());
  }

  // 3. Let result be ? ParseISODateTime(isoString).
  DateTimeRecord result;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, result, ParseISODateTime(isolate, iso_string, *parsed),
      Nothing<TimeRecord>());
  // 4. Return the Record { [[Hour]]: result.[[Hour]], [[Minute]]:
  // result.[[Minute]], [[Second]]: result.[[Second]], [[Millisecond]]:
  // result.[[Millisecond]], [[Microsecond]]: result.[[Microsecond]],
  // [[Nanosecond]]: result.[[Nanosecond]], [[Calendar]]: result.[[Calendar]] }.
  TimeRecord ret = {result.time, result.calendar};
  return Just(ret);
}

// #sec-temporal-parsetemporalinstantstring
Maybe<InstantRecord> ParseTemporalInstantString(Isolate* isolate,
                                                Handle<String> iso_string) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: Type(isoString) is String.
  // 2. If isoString does not satisfy the syntax of a TemporalInstantString
  // (see 13.33), then
  base::Optional<ParsedISO8601Result> parsed =
      TemporalParser::ParseTemporalInstantString(isolate, iso_string);
  if (!parsed.has_value()) {
    THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                 NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                                 Nothing<InstantRecord>());
  }

  // 3. Let result be ! ParseISODateTime(isoString).
  DateTimeRecord result;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, result, ParseISODateTime(isolate, iso_string, *parsed),
      Nothing<InstantRecord>());

  // 4. Let timeZoneResult be ? ParseTemporalTimeZoneString(isoString).
  TimeZoneRecord time_zone_result;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, time_zone_result,
      ParseTemporalTimeZoneString(isolate, iso_string),
      Nothing<InstantRecord>());
  // 5. Let offsetString be timeZoneResult.[[OffsetString]].
  Handle<String> offset_string = time_zone_result.offset_string;
  // 6. If timeZoneResult.[[Z]] is true, then
  if (time_zone_result.z) {
    // a. Set offsetString to "+00:00".
    offset_string = isolate->factory()->NewStringFromStaticChars("+00:00");
  }
  // 7. Assert: offsetString is not undefined.
  DCHECK_GT(offset_string->length(), 0);

  // 6. Return the new Record { [[Year]]: result.[[Year]],
  // [[Month]]: result.[[Month]], [[Day]]: result.[[Day]],
  // [[Hour]]: result.[[Hour]], [[Minute]]: result.[[Minute]],
  // [[Second]]: result.[[Second]],
  // [[Millisecond]]: result.[[Millisecond]],
  // [[Microsecond]]: result.[[Microsecond]],
  // [[Nanosecond]]: result.[[Nanosecond]],
  // [[TimeZoneOffsetString]]: offsetString }.
  InstantRecord record({result.date, result.time, offset_string});
  return Just(record);
}

// #sec-temporal-parsetemporalinstant
MaybeHandle<BigInt> ParseTemporalInstant(Isolate* isolate,
                                         Handle<String> iso_string) {
  TEMPORAL_ENTER_FUNC();

  Factory* factory = isolate->factory();
  // 1. Assert: Type(isoString) is String.
  // 2. Let result be ? ParseTemporalInstantString(isoString).
  InstantRecord result;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, result, ParseTemporalInstantString(isolate, iso_string),
      Handle<BigInt>());

  // 3. Let offsetString be result.[[TimeZoneOffsetString]].
  // 4. Assert: offsetString is not undefined.
  DCHECK_NE(result.offset_string->length(), 0);

  // 5. Let utc be ? GetEpochFromISOParts(result.[[Year]], result.[[Month]],
  // result.[[Day]], result.[[Hour]], result.[[Minute]], result.[[Second]],
  // result.[[Millisecond]], result.[[Microsecond]], result.[[Nanosecond]]).
  Handle<BigInt> utc =
      GetEpochFromISOParts(isolate, {result.date, result.time});

  // 6. If utc < −8.64 × 10^21 or utc > 8.64 × 10^21, then
  if ((BigInt::CompareToNumber(utc, factory->NewNumber(-8.64e21)) ==
       ComparisonResult::kLessThan) ||
      (BigInt::CompareToNumber(utc, factory->NewNumber(8.64e21)) ==
       ComparisonResult::kGreaterThan)) {
    // a. Throw a RangeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(), BigInt);
  }
  // 7. Let offsetNanoseconds be ? ParseTimeZoneOffsetString(offsetString).
  int64_t offset_nanoseconds;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, offset_nanoseconds,
      ParseTimeZoneOffsetString(isolate, result.offset_string),
      Handle<BigInt>());

  // 8. Return utc − offsetNanoseconds.
  return BigInt::Subtract(isolate, utc,
                          BigInt::FromInt64(isolate, offset_nanoseconds));
}

// #sec-temporal-createdurationrecord
Maybe<DurationRecord> CreateDurationRecord(Isolate* isolate,
                                           const DurationRecord& duration) {
  //   1. If ! IsValidDuration(years, months, weeks, days, hours, minutes,
  //   seconds, milliseconds, microseconds, nanoseconds) is false, throw a
  //   RangeError exception.
  if (!IsValidDuration(isolate, duration)) {
    THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                 NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                                 Nothing<DurationRecord>());
  }
  // 2. Return the Record { [[Years]]: ℝ(𝔽(years)), [[Months]]: ℝ(𝔽(months)),
  // [[Weeks]]: ℝ(𝔽(weeks)), [[Days]]: ℝ(𝔽(days)), [[Hours]]: ℝ(𝔽(hours)),
  // [[Minutes]]: ℝ(𝔽(minutes)), [[Seconds]]: ℝ(𝔽(seconds)), [[Milliseconds]]:
  // ℝ(𝔽(milliseconds)), [[Microseconds]]: ℝ(𝔽(microseconds)), [[Nanoseconds]]:
  // ℝ(𝔽(nanoseconds)) }.
  return Just(duration);
}

inline int64_t IfEmptyReturnZero(int64_t value) {
  return value == ParsedISO8601Duration::kEmpty ? 0 : value;
}

// #sec-temporal-parsetemporaldurationstring
Maybe<DurationRecord> ParseTemporalDurationString(Isolate* isolate,
                                                  Handle<String> iso_string) {
  TEMPORAL_ENTER_FUNC();
  // In this funciton, we use 'double' as type for all mathematical values
  // except the three three units < seconds. For all others, we use 'double'
  // because in
  // https://tc39.es/proposal-temporal/#sec-properties-of-temporal-duration-instances
  // they are "A float64-representable integer representing the number" in the
  // internal slots.
  // For milliseconds_mv, microseconds_mv, and nanoseconds_mv, we use int32_t
  // instead because their maximum number during calculation is 999999999,
  // which can be encoded in 30 bits and the parsed.seconds_fraction return from
  // the ISO8601 parser are stored in an integer, in the unit of nanoseconds.
  // Therefore, use "int32_t" will avoid rounding error for the final
  // calculating of nanoseconds_mv.
  //
  // 1. Let duration be ParseText(StringToCodePoints(isoString),
  // TemporalDurationString).
  // 2. If duration is a List of errors, throw a RangeError exception.
  // 3. Let each of sign, years, months, weeks, days, hours, fHours, minutes,
  // fMinutes, seconds, and fSeconds be the source text matched by the
  // respective Sign, DurationYears, DurationMonths, DurationWeeks,
  // DurationDays, DurationWholeHours, DurationHoursFraction,
  // DurationWholeMinutes, DurationMinutesFraction, DurationWholeSeconds, and
  // DurationSecondsFraction Parse Node enclosed by duration, or an empty
  // sequence of code points if not present.
  base::Optional<ParsedISO8601Duration> parsed =
      TemporalParser::ParseTemporalDurationString(isolate, iso_string);
  if (!parsed.has_value()) {
    THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                 NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                                 Nothing<DurationRecord>());
  }
  // 4. Let yearsMV be ! ToIntegerOrInfinity(CodePointsToString(years)).
  double years_mv = IfEmptyReturnZero(parsed->years);
  // 5. Let monthsMV be ! ToIntegerOrInfinity(CodePointsToString(months)).
  double months_mv = IfEmptyReturnZero(parsed->months);
  // 6. Let weeksMV be ! ToIntegerOrInfinity(CodePointsToString(weeks)).
  double weeks_mv = IfEmptyReturnZero(parsed->weeks);
  // 7. Let daysMV be ! ToIntegerOrInfinity(CodePointsToString(days)).
  double days_mv = IfEmptyReturnZero(parsed->days);
  // 8. Let hoursMV be ! ToIntegerOrInfinity(CodePointsToString(hours)).
  double hours_mv = IfEmptyReturnZero(parsed->whole_hours);
  // 9. If fHours is not empty, then
  double minutes_mv;
  if (parsed->hours_fraction != ParsedISO8601Duration::kEmpty) {
    // a. If any of minutes, fMinutes, seconds, fSeconds is not empty, throw a
    // RangeError exception.
    if (parsed->whole_minutes != ParsedISO8601Duration::kEmpty ||
        parsed->minutes_fraction != ParsedISO8601Duration::kEmpty ||
        parsed->whole_seconds != ParsedISO8601Duration::kEmpty ||
        parsed->seconds_fraction != ParsedISO8601Duration::kEmpty) {
      THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                   NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                                   Nothing<DurationRecord>());
    }
    // b. Let fHoursDigits be the substring of CodePointsToString(fHours)
    // from 1. c. Let fHoursScale be the length of fHoursDigits. d. Let
    // minutesMV be ! ToIntegerOrInfinity(fHoursDigits) / 10^fHoursScale × 60.
    minutes_mv = IfEmptyReturnZero(parsed->hours_fraction) * 60.0 / 1e9;
    // 10. Else,
  } else {
    // a. Let minutesMV be ! ToIntegerOrInfinity(CodePointsToString(minutes)).
    minutes_mv = IfEmptyReturnZero(parsed->whole_minutes);
  }
  double seconds_mv;
  // 11. If fMinutes is not empty, then
  if (parsed->minutes_fraction != ParsedISO8601Duration::kEmpty) {
    // a. If any of seconds, fSeconds is not empty, throw a RangeError
    // exception.
    if (parsed->whole_seconds != ParsedISO8601Duration::kEmpty ||
        parsed->seconds_fraction != ParsedISO8601Duration::kEmpty) {
      THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                   NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                                   Nothing<DurationRecord>());
    }
    // b. Let fMinutesDigits be the substring of CodePointsToString(fMinutes)
    // from 1. c. Let fMinutesScale be the length of fMinutesDigits. d. Let
    // secondsMV be ! ToIntegerOrInfinity(fMinutesDigits) / 10^fMinutesScale
    // × 60.
    seconds_mv = IfEmptyReturnZero(parsed->minutes_fraction) * 60.0 / 1e9;
    // 12. Else if seconds is not empty, then
  } else if (parsed->whole_seconds != ParsedISO8601Duration::kEmpty) {
    // a. Let secondsMV be ! ToIntegerOrInfinity(CodePointsToString(seconds)).
    seconds_mv = parsed->whole_seconds;
    // 13. Else,
  } else {
    // a. Let secondsMV be remainder(minutesMV, 1) × 60.
    seconds_mv = (minutes_mv - std::floor(minutes_mv)) * 60.0;
  }
  int32_t milliseconds_mv;
  int32_t nanoseconds_mv;
  // 14. If fSeconds is not empty, then
  if (parsed->seconds_fraction != ParsedISO8601Duration::kEmpty) {
    // a. Let fSecondsDigits be the substring of CodePointsToString(fSeconds)
    // from 1. b. Let fSecondsScale be the length of fSecondsDigits. c. Let
    // millisecondsMV be ! ToIntegerOrInfinity(fSecondsDigits) /
    // 10^fSecondsScale × 1000.
    DCHECK_LE(IfEmptyReturnZero(parsed->seconds_fraction), 1e9);
    nanoseconds_mv =
        static_cast<int32_t>(IfEmptyReturnZero(parsed->seconds_fraction));
    // 15. Else,
  } else {
    // a. Let millisecondsMV be remainder(secondsMV, 1) × 1000.
    nanoseconds_mv = (seconds_mv - std::floor(seconds_mv)) * 1000000000;
  }
  milliseconds_mv = nanoseconds_mv / 1000000;
  // 16. Let microsecondsMV be remainder(millisecondsMV, 1) × 1000.
  int32_t microseconds_mv = (nanoseconds_mv / 1000) % 1000;
  // 17. Let nanosecondsMV be remainder(microsecondsMV, 1) × 1000.
  nanoseconds_mv = nanoseconds_mv % 1000;

  DCHECK_LE(milliseconds_mv, 1000);
  DCHECK_LE(microseconds_mv, 1000);
  DCHECK_LE(nanoseconds_mv, 1000);
  // 18. If sign contains the code point 0x002D (HYPHEN-MINUS) or 0x2212 (MINUS
  // SIGN), then a. Let factor be −1.
  // 19. Else,
  // a. Let factor be 1.
  double factor = parsed->sign;

  // 20. Return ? CreateDurationRecord(yearsMV × factor, monthsMV × factor,
  // weeksMV × factor, daysMV × factor, hoursMV × factor, floor(minutesMV) ×
  // factor, floor(secondsMV) × factor, floor(millisecondsMV) × factor,
  // floor(microsecondsMV) × factor, floor(nanosecondsMV) × factor).

  return CreateDurationRecord(
      isolate,
      {years_mv * factor,
       months_mv * factor,
       weeks_mv * factor,
       {days_mv * factor, hours_mv * factor, std::floor(minutes_mv) * factor,
        std::floor(seconds_mv) * factor, milliseconds_mv * factor,
        microseconds_mv * factor, nanoseconds_mv * factor}});
}

// #sec-temporal-parsetemporaltimezonestring
Maybe<TimeZoneRecord> ParseTemporalTimeZoneString(Isolate* isolate,
                                                  Handle<String> iso_string) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: Type(isoString) is String.
  // 2. If isoString does not satisfy the syntax of a TemporalTimeZoneString
  // (see 13.33), then
  base::Optional<ParsedISO8601Result> parsed =
      TemporalParser::ParseTemporalTimeZoneString(isolate, iso_string);
  if (!parsed.has_value()) {
    THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                 NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                                 Nothing<TimeZoneRecord>());
  }
  // 3. Let z, sign, hours, minutes, seconds, fraction and name be the parts of
  // isoString produced respectively by the UTCDesignator,
  // TimeZoneUTCOffsetSign, TimeZoneUTCOffsetHour, TimeZoneUTCOffsetMinute,
  // TimeZoneUTCOffsetSecond, TimeZoneUTCOffsetFraction, and TimeZoneIANAName
  // productions, or undefined if not present.
  // 4. If z is not undefined, then
  if (parsed->utc_designator) {
    // a. Return the Record { [[Z]]: true, [[OffsetString]]: undefined,
    // [[Name]]: name }.
    if (parsed->tzi_name_length > 0) {
      Handle<String> name = isolate->factory()->NewSubString(
          iso_string, parsed->tzi_name_start,
          parsed->tzi_name_start + parsed->tzi_name_length);
      TimeZoneRecord ret({true, isolate->factory()->empty_string(), name});
      return Just(ret);
    }
    TimeZoneRecord ret({true, isolate->factory()->empty_string(),
                        isolate->factory()->empty_string()});
    return Just(ret);
  }

  // 5. If hours is undefined, then
  // a. Let offsetString be undefined.
  // 6. Else,
  Handle<String> offset_string;
  bool offset_string_is_defined = false;
  if (!parsed->tzuo_hour_is_undefined()) {
    // a. Assert: sign is not undefined.
    DCHECK(!parsed->tzuo_sign_is_undefined());
    // b. Set hours to ! ToIntegerOrInfinity(hours).
    int32_t hours = parsed->tzuo_hour;
    // c. If sign is the code unit 0x002D (HYPHEN-MINUS) or the code unit 0x2212
    // (MINUS SIGN), then i. Set sign to −1. d. Else, i. Set sign to 1.
    int32_t sign = parsed->tzuo_sign;
    // e. Set minutes to ! ToIntegerOrInfinity(minutes).
    int32_t minutes =
        parsed->tzuo_minute_is_undefined() ? 0 : parsed->tzuo_minute;
    // f. Set seconds to ! ToIntegerOrInfinity(seconds).
    int32_t seconds =
        parsed->tzuo_second_is_undefined() ? 0 : parsed->tzuo_second;
    // g. If fraction is not undefined, then
    int32_t nanoseconds;
    if (!parsed->tzuo_nanosecond_is_undefined()) {
      // i. Set fraction to the string-concatenation of the previous value of
      // fraction and the string "000000000".
      // ii. Let nanoseconds be the String value equal to the substring of
      // fraction from 0 to 9. iii. Set nanoseconds to !
      // ToIntegerOrInfinity(nanoseconds).
      nanoseconds = parsed->tzuo_nanosecond;
      // h. Else,
    } else {
      // i. Let nanoseconds be 0.
      nanoseconds = 0;
    }
    // i. Let offsetNanoseconds be sign × (((hours × 60 + minutes) × 60 +
    // seconds) × 10^9 + nanoseconds).
    int64_t offset_nanoseconds =
        sign *
        (((hours * 60 + minutes) * 60 + seconds) * 1000000000L + nanoseconds);
    // j. Let offsetString be ! FormatTimeZoneOffsetString(offsetNanoseconds).
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, offset_string,
        FormatTimeZoneOffsetString(isolate, offset_nanoseconds),
        Nothing<TimeZoneRecord>());
    offset_string_is_defined = true;
  }
  // 7. If name is not undefined, then
  Handle<String> name;
  if (parsed->tzi_name_length > 0) {
    name = isolate->factory()->NewSubString(
        iso_string, parsed->tzi_name_start,
        parsed->tzi_name_start + parsed->tzi_name_length);

    // a. If ! IsValidTimeZoneName(name) is false, throw a RangeError exception.
    if (!IsValidTimeZoneName(isolate, name)) {
      THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                   NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                                   Nothing<TimeZoneRecord>());
    }
    // b. Set name to ! CanonicalizeTimeZoneName(name).
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, name,
                                     CanonicalizeTimeZoneName(isolate, name),
                                     Nothing<TimeZoneRecord>());
    // 8. Return the Record { [[Z]]: false, [[OffsetString]]: offsetString,
    // [[Name]]: name }.
    TimeZoneRecord ret({false,
                        offset_string_is_defined
                            ? offset_string
                            : isolate->factory()->empty_string(),
                        name});
    return Just(ret);
  }
  // 8. Return the Record { [[Z]]: false, [[OffsetString]]: offsetString,
  // [[Name]]: name }.
  TimeZoneRecord ret({false,
                      offset_string_is_defined
                          ? offset_string
                          : isolate->factory()->empty_string(),
                      isolate->factory()->empty_string()});
  return Just(ret);
}

// #sec-temporal-parsetemporaltimezone
MaybeHandle<String> ParseTemporalTimeZone(Isolate* isolate,
                                          Handle<String> string) {
  TEMPORAL_ENTER_FUNC();

  // 2. Let result be ? ParseTemporalTimeZoneString(string).
  TimeZoneRecord result;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, result, ParseTemporalTimeZoneString(isolate, string),
      Handle<String>());

  // 3. If result.[[Name]] is not undefined, return result.[[Name]].
  if (result.name->length() > 0) {
    return result.name;
  }

  // 4. If result.[[Z]] is true, return "UTC".
  if (result.z) {
    return isolate->factory()->UTC_string();
  }

  // 5. Return result.[[OffsetString]].
  return result.offset_string;
}

Maybe<int64_t> ParseTimeZoneOffsetString(Isolate* isolate,
                                         Handle<String> iso_string,
                                         bool throw_if_not_satisfy) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: Type(offsetString) is String.
  // 2. If offsetString does not satisfy the syntax of a
  // TimeZoneNumericUTCOffset (see 13.33), then
  base::Optional<ParsedISO8601Result> parsed =
      TemporalParser::ParseTimeZoneNumericUTCOffset(isolate, iso_string);
  if (throw_if_not_satisfy && !parsed.has_value()) {
    /* a. Throw a RangeError exception. */
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(), Nothing<int64_t>());
  }
  // 3. Let sign, hours, minutes, seconds, and fraction be the parts of
  // offsetString produced respectively by the TimeZoneUTCOffsetSign,
  // TimeZoneUTCOffsetHour, TimeZoneUTCOffsetMinute, TimeZoneUTCOffsetSecond,
  // and TimeZoneUTCOffsetFraction productions, or undefined if not present.
  // 4. If either hours or sign are undefined, throw a RangeError exception.
  if (parsed->tzuo_hour_is_undefined() || parsed->tzuo_sign_is_undefined()) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(), Nothing<int64_t>());
  }
  // 5. If sign is the code unit 0x002D (HYPHEN-MINUS) or 0x2212 (MINUS SIGN),
  // then a. Set sign to −1.
  // 6. Else,
  // a. Set sign to 1.
  int64_t sign = parsed->tzuo_sign;

  // 7. Set hours to ! ToIntegerOrInfinity(hours).
  int64_t hours = parsed->tzuo_hour;
  // 8. Set minutes to ! ToIntegerOrInfinity(minutes).
  int64_t minutes =
      parsed->tzuo_minute_is_undefined() ? 0 : parsed->tzuo_minute;
  // 9. Set seconds to ! ToIntegerOrInfinity(seconds).
  int64_t seconds =
      parsed->tzuo_second_is_undefined() ? 0 : parsed->tzuo_second;
  // 10. If fraction is not undefined, then
  int64_t nanoseconds;
  if (!parsed->tzuo_nanosecond_is_undefined()) {
    // a. Set fraction to the string-concatenation of the previous value of
    // fraction and the string "000000000".
    // b. Let nanoseconds be the String value equal to the substring of fraction
    // consisting of the code units with indices 0 (inclusive) through 9
    // (exclusive). c. Set nanoseconds to ! ToIntegerOrInfinity(nanoseconds).
    nanoseconds = parsed->tzuo_nanosecond;
    // 11. Else,
  } else {
    // a. Let nanoseconds be 0.
    nanoseconds = 0;
  }
  // 12. Return sign × (((hours × 60 + minutes) × 60 + seconds) × 10^9 +
  // nanoseconds).
  return Just(sign * (((hours * 60 + minutes) * 60 + seconds) * 1000000000 +
                      nanoseconds));
}

Maybe<bool> IsValidTimeZoneNumericUTCOffsetString(Isolate* isolate,
                                                  Handle<String> iso_string) {
  TEMPORAL_ENTER_FUNC();

  base::Optional<ParsedISO8601Result> parsed =
      TemporalParser::ParseTimeZoneNumericUTCOffset(isolate, iso_string);
  return Just(parsed.has_value());
}

// #sec-temporal-parsetemporalcalendarstring
MaybeHandle<String> ParseTemporalCalendarString(Isolate* isolate,
                                                Handle<String> iso_string) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: Type(isoString) is String.
  // 2. If isoString does not satisfy the syntax of a TemporalCalendarString
  // (see 13.33), then a. Throw a RangeError exception.
  base::Optional<ParsedISO8601Result> parsed =
      TemporalParser::ParseTemporalCalendarString(isolate, iso_string);
  if (!parsed.has_value()) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(), String);
  }
  // 3. Let id be the part of isoString produced by the CalendarName production,
  // or undefined if not present.
  // 4. If id is undefined, then
  if (parsed->calendar_name_length == 0) {
    // a. Return "iso8601".
    return isolate->factory()->iso8601_string();
  }
  Handle<String> id = isolate->factory()->NewSubString(
      iso_string, parsed->calendar_name_start,
      parsed->calendar_name_start + parsed->calendar_name_length);
  // 5. If ! IsBuiltinCalendar(id) is false, then
  if (!IsBuiltinCalendar(isolate, id)) {
    // a. Throw a RangeError exception.
    THROW_NEW_ERROR(
        isolate, NewRangeError(MessageTemplate::kInvalidCalendar, id), String);
  }
  // 6. Return id.
  return id;
}

// #sec-temporal-calendarfields
MaybeHandle<FixedArray> CalendarFields(Isolate* isolate,
                                       Handle<JSReceiver> calendar,
                                       Handle<FixedArray> field_names) {
  // 1. Let fields be ? GetMethod(calendar, "fields").
  Handle<Object> fields;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, fields,
      Object::GetMethod(calendar, isolate->factory()->fields_string()),
      FixedArray);
  // 2. Let fieldsArray be ! CreateArrayFromList(fieldNames).
  Handle<Object> fields_array =
      isolate->factory()->NewJSArrayWithElements(field_names);
  // 3. If fields is not undefined, then
  if (!fields->IsUndefined()) {
    // a. Set fieldsArray to ? Call(fields, calendar, « fieldsArray »).
    Handle<Object> argv[] = {fields_array};
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, fields_array,
        Execution::Call(isolate, fields, calendar, 1, argv), FixedArray);
  }
  // 4. Return ? IterableToListOfType(fieldsArray, « String »).
  Handle<Object> argv[] = {fields_array};
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, fields_array,
      Execution::CallBuiltin(isolate,
                             isolate->string_fixed_array_from_iterable(),
                             fields_array, 1, argv),
      FixedArray);
  DCHECK(fields_array->IsFixedArray());
  return Handle<FixedArray>::cast(fields_array);
}

MaybeHandle<JSTemporalPlainDate> CalendarDateAdd(Isolate* isolate,
                                                 Handle<JSReceiver> calendar,
                                                 Handle<Object> date,
                                                 Handle<Object> duration,
                                                 Handle<Object> options) {
  return CalendarDateAdd(isolate, calendar, date, duration, options,
                         isolate->factory()->undefined_value());
}

MaybeHandle<JSTemporalPlainDate> CalendarDateAdd(
    Isolate* isolate, Handle<JSReceiver> calendar, Handle<Object> date,
    Handle<Object> duration, Handle<Object> options, Handle<Object> date_add) {
  // 1. Assert: Type(options) is Object or Undefined.
  DCHECK(options->IsJSReceiver() || options->IsUndefined());
  // 2. If dateAdd is not present, set dateAdd to ? GetMethod(calendar,
  // "dateAdd").
  if (date_add->IsUndefined()) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, date_add,
        Object::GetMethod(calendar, isolate->factory()->dateAdd_string()),
        JSTemporalPlainDate);
  }
  // 3. Let addedDate be ? Call(dateAdd, calendar, « date, duration, options »).
  Handle<Object> argv[] = {date, duration, options};
  Handle<Object> added_date;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, added_date,
      Execution::Call(isolate, date_add, calendar, arraysize(argv), argv),
      JSTemporalPlainDate);
  // 4. Perform ? RequireInternalSlot(addedDate, [[InitializedTemporalDate]]).
  if (!added_date->IsJSTemporalPlainDate()) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_TYPE_ERROR(),
                    JSTemporalPlainDate);
  }
  // 5. Return addedDate.
  return Handle<JSTemporalPlainDate>::cast(added_date);
}

MaybeHandle<JSTemporalDuration> CalendarDateUntil(Isolate* isolate,
                                                  Handle<JSReceiver> calendar,
                                                  Handle<Object> one,
                                                  Handle<Object> two,
                                                  Handle<Object> options) {
  return CalendarDateUntil(isolate, calendar, one, two, options,
                           isolate->factory()->undefined_value());
}

MaybeHandle<JSTemporalDuration> CalendarDateUntil(
    Isolate* isolate, Handle<JSReceiver> calendar, Handle<Object> one,
    Handle<Object> two, Handle<Object> options, Handle<Object> date_until) {
  // 1. Assert: Type(calendar) is Object.
  // 2. If dateUntil is not present, set dateUntil to ? GetMethod(calendar,
  // "dateUntil").
  if (date_until->IsUndefined()) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, date_until,
        Object::GetMethod(calendar, isolate->factory()->dateUntil_string()),
        JSTemporalDuration);
  }
  // 3. Let duration be ? Call(dateUntil, calendar, « one, two, options »).
  Handle<Object> argv[] = {one, two, options};
  Handle<Object> duration;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, duration,
      Execution::Call(isolate, date_until, calendar, arraysize(argv), argv),
      JSTemporalDuration);
  // 4. Perform ? RequireInternalSlot(duration,
  // [[InitializedTemporalDuration]]).
  if (!duration->IsJSTemporalDuration()) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_TYPE_ERROR(),
                    JSTemporalDuration);
  }
  // 5. Return duration.
  return Handle<JSTemporalDuration>::cast(duration);
}

// #sec-temporal-defaultmergefields
MaybeHandle<JSReceiver> DefaultMergeFields(
    Isolate* isolate, Handle<JSReceiver> fields,
    Handle<JSReceiver> additional_fields) {
  Factory* factory = isolate->factory();
  // 1. Let merged be ! OrdinaryObjectCreate(%Object.prototype%).
  Handle<JSObject> merged =
      isolate->factory()->NewJSObject(isolate->object_function());

  // 2. Let originalKeys be ? EnumerableOwnPropertyNames(fields, key).
  Handle<FixedArray> original_keys;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, original_keys,
      KeyAccumulator::GetKeys(isolate, fields, KeyCollectionMode::kOwnOnly,
                              ENUMERABLE_STRINGS,
                              GetKeysConversion::kConvertToString),
      JSReceiver);
  // 3. For each element nextKey of originalKeys, do
  for (int i = 0; i < original_keys->length(); i++) {
    // a. If nextKey is not "month" or "monthCode", then
    Handle<Object> next_key = handle(original_keys->get(i), isolate);
    DCHECK(next_key->IsString());
    Handle<String> next_key_string = Handle<String>::cast(next_key);
    if (!(String::Equals(isolate, factory->month_string(), next_key_string) ||
          String::Equals(isolate, factory->monthCode_string(),
                         next_key_string))) {
      // i. Let propValue be ? Get(fields, nextKey).
      Handle<Object> prop_value;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, prop_value,
          JSReceiver::GetPropertyOrElement(isolate, fields, next_key_string),
          JSReceiver);
      // ii. If propValue is not undefined, then
      if (!prop_value->IsUndefined()) {
        // 1. Perform ! CreateDataPropertyOrThrow(merged, nextKey,
        // propValue).
        CHECK(JSReceiver::CreateDataProperty(isolate, merged, next_key_string,
                                             prop_value, Just(kDontThrow))
                  .FromJust());
      }
    }
  }
  // 4. Let newKeys be ? EnumerableOwnPropertyNames(additionalFields, key).
  Handle<FixedArray> new_keys;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, new_keys,
      KeyAccumulator::GetKeys(isolate, additional_fields,
                              KeyCollectionMode::kOwnOnly, ENUMERABLE_STRINGS,
                              GetKeysConversion::kConvertToString),
      JSReceiver);
  bool new_keys_has_month_or_month_code = false;
  // 5. For each element nextKey of newKeys, do
  for (int i = 0; i < new_keys->length(); i++) {
    Handle<Object> next_key = handle(new_keys->get(i), isolate);
    DCHECK(next_key->IsString());
    Handle<String> next_key_string = Handle<String>::cast(next_key);
    // a. Let propValue be ? Get(additionalFields, nextKey).
    Handle<Object> prop_value;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, prop_value,
                               JSReceiver::GetPropertyOrElement(
                                   isolate, additional_fields, next_key_string),
                               JSReceiver);
    // b. If propValue is not undefined, then
    if (!prop_value->IsUndefined()) {
      // 1. Perform ! CreateDataPropertyOrThrow(merged, nextKey, propValue).
      CHECK(JSReceiver::CreateDataProperty(isolate, merged, next_key_string,
                                           prop_value, Just(kDontThrow))
                .FromJust());
    }
    new_keys_has_month_or_month_code |=
        String::Equals(isolate, factory->month_string(), next_key_string) ||
        String::Equals(isolate, factory->monthCode_string(), next_key_string);
  }
  // 6. If newKeys does not contain either "month" or "monthCode", then
  if (!new_keys_has_month_or_month_code) {
    // a. Let month be ? Get(fields, "month").
    Handle<Object> month;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, month,
                               JSReceiver::GetPropertyOrElement(
                                   isolate, fields, factory->month_string()),
                               JSReceiver);
    // b. If month is not undefined, then
    if (!month->IsUndefined()) {
      // i. Perform ! CreateDataPropertyOrThrow(merged, "month", month).
      CHECK(JSReceiver::CreateDataProperty(isolate, merged,
                                           factory->month_string(), month,
                                           Just(kDontThrow))
                .FromJust());
    }
    // c. Let monthCode be ? Get(fields, "monthCode").
    Handle<Object> month_code;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, month_code,
        JSReceiver::GetPropertyOrElement(isolate, fields,
                                         factory->monthCode_string()),
        JSReceiver);
    // d. If monthCode is not undefined, then
    if (!month_code->IsUndefined()) {
      // i. Perform ! CreateDataPropertyOrThrow(merged, "monthCode", monthCode).
      CHECK(JSReceiver::CreateDataProperty(isolate, merged,
                                           factory->monthCode_string(),
                                           month_code, Just(kDontThrow))
                .FromJust());
    }
  }
  // 7. Return merged.
  return merged;
}

// #sec-temporal-getoffsetnanosecondsfor
Maybe<int64_t> GetOffsetNanosecondsFor(Isolate* isolate,
                                       Handle<JSReceiver> time_zone_obj,
                                       Handle<Object> instant,
                                       const char* method_name) {
  TEMPORAL_ENTER_FUNC();
  // 1. Let getOffsetNanosecondsFor be ? GetMethod(timeZone,
  // "getOffsetNanosecondsFor").
  Handle<Object> get_offset_nanoseconds_for;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, get_offset_nanoseconds_for,
      Object::GetMethod(time_zone_obj,
                        isolate->factory()->getOffsetNanosecondsFor_string()),
      Nothing<int64_t>());
  if (!get_offset_nanoseconds_for->IsCallable()) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewTypeError(MessageTemplate::kCalledNonCallable,
                     isolate->factory()->getOffsetNanosecondsFor_string()),
        Nothing<int64_t>());
  }
  Handle<Object> offset_nanoseconds_obj;
  // 3. Let offsetNanoseconds be ? Call(getOffsetNanosecondsFor, timeZone, «
  // instant »).
  Handle<Object> argv[] = {instant};
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, offset_nanoseconds_obj,
      Execution::Call(isolate, get_offset_nanoseconds_for, time_zone_obj, 1,
                      argv),
      Nothing<int64_t>());

  // 4. If Type(offsetNanoseconds) is not Number, throw a TypeError exception.
  if (!offset_nanoseconds_obj->IsNumber()) {
    THROW_NEW_ERROR_RETURN_VALUE(isolate, NEW_TEMPORAL_INVALID_ARG_TYPE_ERROR(),
                                 Nothing<int64_t>());
  }

  // 5. If ! IsIntegralNumber(offsetNanoseconds) is false, throw a RangeError
  // exception.
  if (!IsIntegralNumber(isolate, offset_nanoseconds_obj)) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(), Nothing<int64_t>());
  }
  double offset_nanoseconds = offset_nanoseconds_obj->Number();

  // 6. Set offsetNanoseconds to ℝ(offsetNanoseconds).
  int64_t offset_nanoseconds_int = static_cast<int64_t>(offset_nanoseconds);
  // 7. If abs(offsetNanoseconds) > 86400 × 10^9, throw a RangeError exception.
  if (std::abs(offset_nanoseconds_int) > 86400e9) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(), Nothing<int64_t>());
  }
  // 8. Return offsetNanoseconds.
  return Just(offset_nanoseconds_int);
}

// #sec-temporal-topositiveinteger
MaybeHandle<Object> ToPositiveInteger(Isolate* isolate,
                                      Handle<Object> argument) {
  TEMPORAL_ENTER_FUNC();

  // 1. Let integer be ? ToInteger(argument).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, argument, ToIntegerThrowOnInfinity(isolate, argument), Object);
  // 2. If integer ≤ 0, then
  if (NumberToInt32(*argument) <= 0) {
    // a. Throw a RangeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(), Object);
  }
  return argument;
}

}  // namespace

namespace temporal {
MaybeHandle<Object> InvokeCalendarMethod(Isolate* isolate,
                                         Handle<JSReceiver> calendar,
                                         Handle<String> name,
                                         Handle<JSReceiver> date_like) {
  Handle<Object> result;
  /* 1. Assert: Type(calendar) is Object. */
  DCHECK(calendar->IsObject());
  /* 2. Let result be ? Invoke(calendar, #name, « dateLike »). */
  Handle<Object> function;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, function, Object::GetProperty(isolate, calendar, name), Object);
  if (!function->IsCallable()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kCalledNonCallable, name),
                    Object);
  }
  Handle<Object> argv[] = {date_like};
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      Execution::Call(isolate, function, calendar, arraysize(argv), argv),
      Object);
  return result;
}

#define CALENDAR_ABSTRACT_OPERATION_INT_ACTION(Name, name, Action)            \
  MaybeHandle<Object> Calendar##Name(Isolate* isolate,                        \
                                     Handle<JSReceiver> calendar,             \
                                     Handle<JSReceiver> date_like) {          \
    /* 1. Assert: Type(calendar) is Object.   */                              \
    /* 2. Let result be ? Invoke(calendar, property, « dateLike »). */      \
    Handle<Object> result;                                                    \
    ASSIGN_RETURN_ON_EXCEPTION(                                               \
        isolate, result,                                                      \
        InvokeCalendarMethod(isolate, calendar,                               \
                             isolate->factory()->name##_string(), date_like), \
        Object);                                                              \
    /* 3. If result is undefined, throw a RangeError exception. */            \
    if (result->IsUndefined()) {                                              \
      THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),        \
                      Object);                                                \
    }                                                                         \
    /* 4. Return ? Action(result). */                                         \
    ASSIGN_RETURN_ON_EXCEPTION(isolate, result, Action(isolate, result),      \
                               Object);                                       \
    return Handle<Smi>(Smi::FromInt(result->Number()), isolate);              \
  }

// #sec-temporal-calendaryear
CALENDAR_ABSTRACT_OPERATION_INT_ACTION(Year, year, ToIntegerThrowOnInfinity)
// #sec-temporal-calendarmonth
CALENDAR_ABSTRACT_OPERATION_INT_ACTION(Month, month, ToPositiveInteger)
// #sec-temporal-calendarday
CALENDAR_ABSTRACT_OPERATION_INT_ACTION(Day, day, ToPositiveInteger)
// #sec-temporal-calendarmonthcode
MaybeHandle<Object> CalendarMonthCode(Isolate* isolate,
                                      Handle<JSReceiver> calendar,
                                      Handle<JSReceiver> date_like) {
  // 1. Assert: Type(calendar) is Object.
  // 2. Let result be ? Invoke(calendar, monthCode , « dateLike »).
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      InvokeCalendarMethod(isolate, calendar,
                           isolate->factory()->monthCode_string(), date_like),
      Object);
  /* 3. If result is undefined, throw a RangeError exception. */
  if (result->IsUndefined()) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(), Object);
  }
  // 4. Return ? ToString(result).
  return Object::ToString(isolate, result);
}

#ifdef V8_INTL_SUPPORT
// #sec-temporal-calendarerayear
MaybeHandle<Object> CalendarEraYear(Isolate* isolate,
                                    Handle<JSReceiver> calendar,
                                    Handle<JSReceiver> date_like) {
  // 1. Assert: Type(calendar) is Object.
  // 2. Let result be ? Invoke(calendar, eraYear , « dateLike »).
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      InvokeCalendarMethod(isolate, calendar,
                           isolate->factory()->eraYear_string(), date_like),
      Object);
  // 3. If result is not undefined, set result to ? ToIntegerOrInfinity(result).
  if (!result->IsUndefined()) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result, ToIntegerThrowOnInfinity(isolate, result), Object);
  }
  // 4. Return result.
  return result;
}

// #sec-temporal-calendarera
MaybeHandle<Object> CalendarEra(Isolate* isolate, Handle<JSReceiver> calendar,
                                Handle<JSReceiver> date_like) {
  // 1. Assert: Type(calendar) is Object.
  // 2. Let result be ? Invoke(calendar, era , « dateLike »).
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      InvokeCalendarMethod(isolate, calendar, isolate->factory()->era_string(),
                           date_like),
      Object);
  // 3. If result is not undefined, set result to ? ToString(result).
  if (!result->IsUndefined()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                               Object::ToString(isolate, result), Object);
  }
  // 4. Return result.
  return result;
}

#endif  //  V8_INTL_SUPPORT

// #sec-temporal-getiso8601calendar
MaybeHandle<JSTemporalCalendar> GetISO8601Calendar(Isolate* isolate) {
  return CreateTemporalCalendar(isolate, isolate->factory()->iso8601_string());
}

}  // namespace temporal

namespace {

bool IsUTC(Isolate* isolate, Handle<String> time_zone) {
  // 1. Assert: Type(timeZone) is String.
  // 2. Let tzText be ! StringToCodePoints(timeZone).
  // 3. Let tzUpperText be the result of toUppercase(tzText), according to the
  // Unicode Default Case Conversion algorithm.
  // 4. Let tzUpper be ! CodePointsToString(tzUpperText).
  // 5. If tzUpper and "UTC" are the same sequence of code points, return true.
  // 6. Return false.
  if (time_zone->length() != 3) return false;
  time_zone = String::Flatten(isolate, time_zone);
  DisallowGarbageCollection no_gc;
  const String::FlatContent& flat = time_zone->GetFlatContent(no_gc);
  return (flat.Get(0) == u'U' || flat.Get(0) == u'u') &&
         (flat.Get(1) == u'T' || flat.Get(1) == u't') &&
         (flat.Get(2) == u'C' || flat.Get(2) == u'c');
}

#ifdef V8_INTL_SUPPORT
class CalendarMap final {
 public:
  CalendarMap() {
    icu::Locale locale("und");
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::StringEnumeration> enumeration(
        icu::Calendar::getKeywordValuesForLocale("ca", locale, false, status));
    calendar_ids.push_back("iso8601");
    calendar_id_indices.insert({"iso8601", 0});
    int32_t i = 1;
    for (const char* item = enumeration->next(nullptr, status);
         U_SUCCESS(status) && item != nullptr;
         item = enumeration->next(nullptr, status)) {
      if (strcmp(item, "iso8601") != 0) {
        const char* type = uloc_toUnicodeLocaleType("ca", item);
        calendar_ids.push_back(type);
        calendar_id_indices.insert({type, i++});
      }
    }
  }
  bool Contains(const std::string& id) const {
    return calendar_id_indices.find(id) != calendar_id_indices.end();
  }

  std::string Id(int32_t index) const {
    DCHECK_LT(index, calendar_ids.size());
    return calendar_ids[index];
  }

  int32_t Index(const char* id) const {
    return calendar_id_indices.find(id)->second;
  }

 private:
  std::map<std::string, int32_t> calendar_id_indices;
  std::vector<std::string> calendar_ids;
};

DEFINE_LAZY_LEAKY_OBJECT_GETTER(CalendarMap, GetCalendarMap)

// #sec-temporal-isbuiltincalendar
bool IsBuiltinCalendar(Isolate* isolate, const std::string& id) {
  return GetCalendarMap()->Contains(id);
}

bool IsBuiltinCalendar(Isolate* isolate, Handle<String> id) {
  return IsBuiltinCalendar(isolate, id->ToCString().get());
}

Handle<String> CalendarIdentifier(Isolate* isolate, int32_t index) {
  return isolate->factory()->NewStringFromAsciiChecked(
      GetCalendarMap()->Id(index).c_str());
}

int32_t CalendarIndex(Isolate* isolate, Handle<String> id) {
  return GetCalendarMap()->Index(id->ToCString().get());
}

bool IsValidTimeZoneName(Isolate* isolate, Handle<String> time_zone) {
  return Intl::IsValidTimeZoneName(isolate, time_zone);
}

MaybeHandle<String> CanonicalizeTimeZoneName(Isolate* isolate,
                                             Handle<String> identifier) {
  return Intl::CanonicalizeTimeZoneName(isolate, identifier);
}

#else   // V8_INTL_SUPPORT
Handle<String> CalendarIdentifier(Isolate* isolate, int32_t index) {
  DCHECK_EQ(index, 0);
  return isolate->factory()->iso8601_string();
}

// #sec-temporal-isbuiltincalendar
bool IsBuiltinCalendar(Isolate* isolate, Handle<String> id) {
  // 1. If id is not "iso8601", return false.
  // 2. Return true
  return isolate->factory()->iso8601_string()->Equals(*id);
}

int32_t CalendarIndex(Isolate* isolate, Handle<String> id) { return 0; }
// #sec-isvalidtimezonename
bool IsValidTimeZoneName(Isolate* isolate, Handle<String> time_zone) {
  return IsUTC(isolate, time_zone);
}
// #sec-canonicalizetimezonename
MaybeHandle<String> CanonicalizeTimeZoneName(Isolate* isolate,
                                             Handle<String> identifier) {
  return isolate->factory()->UTC_string();
}
#endif  // V8_INTL_SUPPORT

// #sec-temporal-totemporaltimerecord
Maybe<TimeRecord> ToTemporalTimeRecord(Isolate* isolate,
                                       Handle<JSReceiver> temporal_time_like,
                                       const char* method_name) {
  TEMPORAL_ENTER_FUNC();

  TimeRecord result;
  Factory* factory = isolate->factory();
  // 1. Assert: Type(temporalTimeLike) is Object.
  // 2. Let result be the new Record { [[Hour]]: undefined, [[Minute]]:
  // undefined, [[Second]]: undefined, [[Millisecond]]: undefined,
  // [[Microsecond]]: undefined, [[Nanosecond]]: undefined }.
  // See https://github.com/tc39/proposal-temporal/pull/1862
  // 3. Let _any_ be *false*.
  bool any = false;
  // 4. For each row of Table 3, except the header row, in table order, do
  std::array<std::pair<Handle<String>, int32_t*>, 6> table3 = {
      {{factory->hour_string(), &result.time.hour},
       {factory->microsecond_string(), &result.time.microsecond},
       {factory->millisecond_string(), &result.time.millisecond},
       {factory->minute_string(), &result.time.minute},
       {factory->nanosecond_string(), &result.time.nanosecond},
       {factory->second_string(), &result.time.second}}};
  for (const auto& row : table3) {
    Handle<Object> value;
    // a. Let property be the Property value of the current row.
    // b. Let value be ? Get(temporalTimeLike, property).
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, value,
        Object::GetPropertyOrElement(isolate, temporal_time_like, row.first),
        Nothing<TimeRecord>());
    // c. If value is not undefined, then
    if (!value->IsUndefined()) {
      // i. Set _any_ to *true*.
      any = true;
    }
    // d. Set value to ? ToIntegerThrowOnOInfinity(value).
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, value,
                                     ToIntegerThrowOnInfinity(isolate, value),
                                     Nothing<TimeRecord>());
    // e. Set result's internal slot whose name is the Internal Slot value of
    // the current row to value.
    *(row.second) = value->Number();
  }

  // 5. If _any_ is *false*, then
  if (!any) {
    // a. Throw a *TypeError* exception.
    THROW_NEW_ERROR_RETURN_VALUE(isolate, NEW_TEMPORAL_INVALID_ARG_TYPE_ERROR(),
                                 Nothing<TimeRecord>());
  }
  // 4. Return result.
  return Just(result);
}

// #sec-temporal-mergelargestunitoption
MaybeHandle<JSObject> MergeLargestUnitOption(Isolate* isolate,
                                             Handle<JSReceiver> options,
                                             Unit largest_unit) {
  TEMPORAL_ENTER_FUNC();
  // 1. Let merged be ! OrdinaryObjectCreate(%Object.prototype%).
  Handle<JSObject> merged =
      isolate->factory()->NewJSObject(isolate->object_function());
  // 2. Let keys be ? EnumerableOwnPropertyNames(options, key).
  // 3. For each element nextKey of keys, do
  // a. Let propValue be ? Get(options, nextKey).
  // b. Perform ! CreateDataPropertyOrThrow(merged, nextKey, propValue).
  JSReceiver::SetOrCopyDataProperties(
      isolate, merged, options, PropertiesEnumerationMode::kEnumerationOrder,
      nullptr, false)
      .Check();

  // 4. Perform ! CreateDataPropertyOrThrow(merged, "largestUnit", largestUnit).
  CHECK(JSReceiver::CreateDataProperty(
            isolate, merged, isolate->factory()->largestUnit_string(),
            UnitToString(isolate, largest_unit), Just(kThrowOnError))
            .FromJust());
  // 5. Return merged.
  return merged;
}

// #sec-temporal-tolargesttemporalunit
Maybe<Unit> ToLargestTemporalUnit(Isolate* isolate,
                                  Handle<JSReceiver> normalized_options,
                                  std::set<Unit> disallowed_units,
                                  Unit fallback, Unit auto_value,
                                  const char* method) {
  // 1. Assert: disallowedUnits does not contain fallback or "auto".
  DCHECK_EQ(disallowed_units.find(fallback), disallowed_units.end());
  DCHECK_EQ(disallowed_units.find(Unit::kAuto), disallowed_units.end());
  // 2. Assert: If autoValue is present, fallback is "auto", and disallowedUnits
  // does not contain autoValue.
  DCHECK(auto_value == Unit::kNotPresent || fallback == Unit::kAuto);
  DCHECK(auto_value == Unit::kNotPresent ||
         disallowed_units.find(auto_value) == disallowed_units.end());
  // 3. Let largestUnit be ? GetOption(normalizedOptions, "largestUnit", «
  // String », « "auto", "year", "years", "month", "months", "week", "weeks",
  // "day", "days", "hour", "hours", "minute", "minutes", "second", "seconds",
  // "millisecond", "milliseconds", "microsecond", "microseconds", "nanosecond",
  // "nanoseconds" », fallback).
  Unit largest_unit;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, largest_unit,
      GetStringOption<Unit>(
          isolate, normalized_options, "largestUnit", method,
          {"auto",         "year",        "years",        "month",
           "months",       "week",        "weeks",        "day",
           "days",         "hour",        "hours",        "minute",
           "minutes",      "second",      "seconds",      "millisecond",
           "milliseconds", "microsecond", "microseconds", "nanosecond",
           "nanoseconds"},
          {Unit::kAuto,        Unit::kYear,        Unit::kYear,
           Unit::kMonth,       Unit::kMonth,       Unit::kWeek,
           Unit::kWeek,        Unit::kDay,         Unit::kDay,
           Unit::kHour,        Unit::kHour,        Unit::kMinute,
           Unit::kMinute,      Unit::kSecond,      Unit::kSecond,
           Unit::kMillisecond, Unit::kMillisecond, Unit::kMicrosecond,
           Unit::kMicrosecond, Unit::kNanosecond,  Unit::kNanosecond},
          fallback),
      Nothing<Unit>());
  // 4. If largestUnit is "auto" and autoValue is present, then
  if (largest_unit == Unit::kAuto && auto_value != Unit::kNotPresent) {
    // a. Return autoValue.
    return Just(auto_value);
  }
  // 5. If largestUnit is in the Plural column of Table 12, then
  // a. Set largestUnit to the corresponding Singular value of the same row.
  // 6. If disallowedUnits contains largestUnit, then
  if (disallowed_units.find(largest_unit) != disallowed_units.end()) {
    // a. Throw a RangeError exception.
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(MessageTemplate::kInvalidUnit,
                      isolate->factory()->NewStringFromAsciiChecked(method),
                      isolate->factory()->largestUnit_string()),
        Nothing<Unit>());
  }
  // 7. Return largestUnit.
  return Just(largest_unit);
}

// #sec-temporal-tointegerthrowoninfinity
MaybeHandle<Object> ToIntegerThrowOnInfinity(Isolate* isolate,
                                             Handle<Object> argument) {
  TEMPORAL_ENTER_FUNC();

  // 1. Let integer be ? ToIntegerOrInfinity(argument).
  ASSIGN_RETURN_ON_EXCEPTION(isolate, argument,
                             Object::ToInteger(isolate, argument), Object);
  // 2. If integer is +∞ or -∞, throw a RangeError exception.
  if (!std::isfinite(argument->Number())) {
    // a. Throw a RangeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(), Object);
  }
  return argument;
}

// #sec-temporal-largeroftwotemporalunits
Unit LargerOfTwoTemporalUnits(Isolate* isolate, Unit u1, Unit u2) {
  // 1. If either u1 or u2 is "year", return "year".
  if (u1 == Unit::kYear || u2 == Unit::kYear) return Unit::kYear;
  // 2. If either u1 or u2 is "month", return "month".
  if (u1 == Unit::kMonth || u2 == Unit::kMonth) return Unit::kMonth;
  // 3. If either u1 or u2 is "week", return "week".
  if (u1 == Unit::kWeek || u2 == Unit::kWeek) return Unit::kWeek;
  // 4. If either u1 or u2 is "day", return "day".
  if (u1 == Unit::kDay || u2 == Unit::kDay) return Unit::kDay;
  // 5. If either u1 or u2 is "hour", return "hour".
  if (u1 == Unit::kHour || u2 == Unit::kHour) return Unit::kHour;
  // 6. If either u1 or u2 is "minute", return "minute".
  if (u1 == Unit::kMinute || u2 == Unit::kMinute) return Unit::kMinute;
  // 7. If either u1 or u2 is "second", return "second".
  if (u1 == Unit::kSecond || u2 == Unit::kSecond) return Unit::kSecond;
  // 8. If either u1 or u2 is "millisecond", return "millisecond".
  if (u1 == Unit::kMillisecond || u2 == Unit::kMillisecond)
    return Unit::kMillisecond;
  // 9. If either u1 or u2 is "microsecond", return "microsecond".
  if (u1 == Unit::kMicrosecond || u2 == Unit::kMicrosecond)
    return Unit::kMicrosecond;
  // 10. Return "nanosecond".
  return Unit::kNanosecond;
}

Handle<String> UnitToString(Isolate* isolate, Unit unit) {
  switch (unit) {
    case Unit::kYear:
      return ReadOnlyRoots(isolate).year_string_handle();
    case Unit::kMonth:
      return ReadOnlyRoots(isolate).month_string_handle();
    case Unit::kWeek:
      return ReadOnlyRoots(isolate).week_string_handle();
    case Unit::kDay:
      return ReadOnlyRoots(isolate).day_string_handle();
    case Unit::kHour:
      return ReadOnlyRoots(isolate).hour_string_handle();
    case Unit::kMinute:
      return ReadOnlyRoots(isolate).minute_string_handle();
    case Unit::kSecond:
      return ReadOnlyRoots(isolate).second_string_handle();
    case Unit::kMillisecond:
      return ReadOnlyRoots(isolate).millisecond_string_handle();
    case Unit::kMicrosecond:
      return ReadOnlyRoots(isolate).microsecond_string_handle();
    case Unit::kNanosecond:
      return ReadOnlyRoots(isolate).nanosecond_string_handle();
    case Unit::kNotPresent:
    case Unit::kAuto:
      UNREACHABLE();
  }
}

// #sec-temporal-balanceisodate
DateRecordCommon BalanceISODate(Isolate* isolate,
                                const DateRecordCommon& date) {
  TEMPORAL_ENTER_FUNC();

  DateRecordCommon result = date;
  // 1. Assert: year, month, and day are integers.
  // 2. Let balancedYearMonth be ! BalanceISOYearMonth(year, month).
  // 3. Set month to balancedYearMonth.[[Month]].
  // 4. Set year to balancedYearMonth.[[Year]].
  BalanceISOYearMonth(isolate, &(result.year), &(result.month));
  // 5. NOTE: To deal with negative numbers of days whose absolute value is
  // greater than the number of days in a year, the following section subtracts
  // years and adds days until the number of days is greater than −366 or −365.
  // 6. If month > 2, then
  // a. Let testYear be year.
  // 7. Else,
  // a. Let testYear be year − 1.
  int32_t test_year = (date.month > 2) ? date.year : date.year - 1;
  // 8. Repeat, while day < −1 × ! ISODaysInYear(testYear),
  int32_t iso_days_in_year;
  while (result.day < -(iso_days_in_year = ISODaysInYear(isolate, test_year))) {
    // a. Set day to day + ! ISODaysInYear(testYear).
    result.day += iso_days_in_year;
    // b. Set year to year − 1.
    (result.year)--;
    // c. Set testYear to testYear − 1.
    test_year--;
  }
  // 9. NOTE: To deal with numbers of days greater than the number of days in a
  // year, the following section adds years and subtracts days until the number
  // of days is less than 366 or 365.
  // 10. Let testYear be year + 1.
  test_year = (result.year) + 1;
  // 11. Repeat, while day > ! ISODaysInYear(testYear),
  while (result.day > (iso_days_in_year = ISODaysInYear(isolate, test_year))) {
    // a. Set day to day − ! ISODaysInYear(testYear).
    result.day -= iso_days_in_year;
    // b. Set year to year + 1.
    result.year++;
    // c. Set testYear to testYear + 1.
    test_year++;
  }
  // 12. NOTE: To deal with negative numbers of days whose absolute value is
  // greater than the number of days in the current month, the following section
  // subtracts months and adds days until the number of days is greater than 0.
  // 13. Repeat, while day < 1,
  while (result.day < 1) {
    // a. Set balancedYearMonth to ! BalanceISOYearMonth(year, month − 1).
    // b. Set year to balancedYearMonth.[[Year]].
    // c. Set month to balancedYearMonth.[[Month]].
    result.month -= 1;
    BalanceISOYearMonth(isolate, &(result.year), &(result.month));
    // d. Set day to day + ! ISODaysInMonth(year, month).
    result.day += ISODaysInMonth(isolate, result.year, result.month);
  }
  // 14. NOTE: To deal with numbers of days greater than the number of days in
  // the current month, the following section adds months and subtracts days
  // until the number of days is less than the number of days in the month.
  // 15. Repeat, while day > ! ISODaysInMonth(year, month),
  int32_t iso_days_in_month;
  while (result.day > (iso_days_in_month = ISODaysInMonth(isolate, result.year,
                                                          result.month))) {
    // a. Set day to day − ! ISODaysInMonth(year, month).
    result.day -= iso_days_in_month;
    // b. Set balancedYearMonth to ! BalanceISOYearMonth(year, month + 1).
    // c. Set year to balancedYearMonth.[[Year]].
    // d. Set month to balancedYearMonth.[[Month]].
    result.month += 1;
    BalanceISOYearMonth(isolate, &(result.year), &(result.month));
  }
  // 16. Return the new Record { [[Year]]: year, [[Month]]: month, [[Day]]: day
  // }.
  return result;
}

// #sec-temporal-adddatetime
Maybe<DateTimeRecordCommon> AddDateTime(Isolate* isolate,
                                        const DateTimeRecordCommon& date_time,
                                        Handle<JSReceiver> calendar,
                                        const DurationRecord& dur,
                                        Handle<Object> options) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: year, month, day, hour, minute, second, millisecond,
  // microsecond, and nanosecond are integers.
  // 2. Let timeResult be ! AddTime(hour, minute, second, millisecond,
  // microsecond, nanosecond, hours, minutes, seconds, milliseconds,
  // microseconds, nanoseconds).
  const TimeDurationRecord& time = dur.time_duration;
  DateTimeRecordCommon time_result =
      AddTime(isolate, date_time.time,
              {0, time.hours, time.minutes, time.seconds, time.milliseconds,
               time.microseconds, time.nanoseconds});

  // 3. Let datePart be ? CreateTemporalDate(year, month, day, calendar).
  Handle<JSTemporalPlainDate> date_part;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, date_part, CreateTemporalDate(isolate, date_time.date, calendar),
      Nothing<DateTimeRecordCommon>());
  // 4. Let dateDuration be ? CreateTemporalDuration(years, months, weeks, days
  // + timeResult.[[Days]], 0, 0, 0, 0, 0, 0).
  Handle<JSTemporalDuration> date_duration;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, date_duration,
      CreateTemporalDuration(
          isolate,
          {dur.years,
           dur.months,
           dur.weeks,
           {dur.time_duration.days + time_result.date.day, 0, 0, 0, 0, 0, 0}}),
      Nothing<DateTimeRecordCommon>());
  // 5. Let addedDate be ? CalendarDateAdd(calendar, datePart, dateDuration,
  // options).
  Handle<JSTemporalPlainDate> added_date;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, added_date,
      CalendarDateAdd(isolate, calendar, date_part, date_duration, options),
      Nothing<DateTimeRecordCommon>());
  // 6. Return the new Record { [[Year]]: addedDate.[[ISOYear]], [[Month]]:
  // addedDate.[[ISOMonth]], [[Day]]: addedDate.[[ISODay]], [[Hour]]:
  // timeResult.[[Hour]], [[Minute]]: timeResult.[[Minute]], [[Second]]:
  // timeResult.[[Second]], [[Millisecond]]: timeResult.[[Millisecond]],
  // [[Microsecond]]: timeResult.[[Microsecond]], [[Nanosecond]]:
  // timeResult.[[Nanosecond]], }.
  time_result.date = {added_date->iso_year(), added_date->iso_month(),
                      added_date->iso_day()};
  return Just(time_result);
}

Maybe<TimeDurationRecord> BalanceDuration(Isolate* isolate, Unit largest_unit,
                                          const TimeDurationRecord& duration,
                                          const char* method_name) {
  TEMPORAL_ENTER_FUNC();

  // 1. If relativeTo is not present, set relativeTo to undefined.
  return BalanceDuration(isolate, largest_unit,
                         isolate->factory()->undefined_value(), duration,
                         method_name);
}

Maybe<TimeDurationRecord> BalanceDuration(Isolate* isolate, Unit largest_unit,
                                          Handle<Object> relative_to_obj,
                                          const TimeDurationRecord& value,
                                          const char* method_name) {
  TEMPORAL_ENTER_FUNC();
  TimeDurationRecord duration = value;

  // 2. If Type(relativeTo) is Object and relativeTo has an
  // [[InitializedTemporalZonedDateTime]] internal slot, then
  if (relative_to_obj->IsJSTemporalZonedDateTime()) {
    Handle<JSTemporalZonedDateTime> relative_to =
        Handle<JSTemporalZonedDateTime>::cast(relative_to_obj);
    // a. Let endNs be ? AddZonedDateTime(relativeTo.[[Nanoseconds]],
    // relativeTo.[[TimeZone]], relativeTo.[[Calendar]], 0, 0, 0, days, hours,
    // minutes, seconds, milliseconds, microseconds, nanoseconds).
    Handle<BigInt> end_ns;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, end_ns,
        AddZonedDateTime(isolate,
                         Handle<BigInt>(relative_to->nanoseconds(), isolate),
                         Handle<JSReceiver>(relative_to->time_zone(), isolate),
                         Handle<JSReceiver>(relative_to->calendar(), isolate),
                         {0, 0, 0, duration}, method_name),
        Nothing<TimeDurationRecord>());
    // b. Set nanoseconds to endNs − relativeTo.[[Nanoseconds]].
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, end_ns,
        BigInt::Subtract(isolate, end_ns,
                         Handle<BigInt>(relative_to->nanoseconds(), isolate)),
        Nothing<TimeDurationRecord>());
    duration.nanoseconds = end_ns->AsInt64();
    // 3. Else,
  } else {
    // a. Set nanoseconds to ℤ(! TotalDurationNanoseconds(days, hours, minutes,
    // seconds, milliseconds, microseconds, nanoseconds, 0)).
    duration.nanoseconds = TotalDurationNanoseconds(isolate, duration, 0);
  }
  // 4. If largestUnit is one of "year", "month", "week", or "day", then
  if (largest_unit == Unit::kYear || largest_unit == Unit::kMonth ||
      largest_unit == Unit::kWeek || largest_unit == Unit::kDay) {
    // a. Let result be ? NanosecondsToDays(nanoseconds, relativeTo).
    NanosecondsToDaysResult result;
    MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, result,
        NanosecondsToDays(isolate, duration.nanoseconds, relative_to_obj,
                          method_name),
        Nothing<TimeDurationRecord>());
    duration.days = result.days;
    // b. Set days to result.[[Days]].
    // c. Set nanoseconds to result.[[Nanoseconds]].
    duration.nanoseconds = result.nanoseconds;
    // 5. Else,
  } else {
    // a. Set days to 0.
    duration.days = 0;
  }
  // 6. Set hours, minutes, seconds, milliseconds, and microseconds to 0.
  duration.hours = duration.minutes = duration.seconds = duration.milliseconds =
      duration.microseconds = 0;
  // 7. If nanoseconds < 0, let sign be −1; else, let sign be 1.
  int32_t sign = (duration.nanoseconds < 0) ? -1 : 1;
  // 8. Set nanoseconds to abs(nanoseconds).
  duration.nanoseconds = std::abs(duration.nanoseconds);
  // 9 If largestUnit is "year", "month", "week", "day", or "hour", then
  switch (largest_unit) {
    case Unit::kYear:
    case Unit::kMonth:
    case Unit::kWeek:
    case Unit::kDay:
    case Unit::kHour:
      // a. Set microseconds to floor(nanoseconds / 1000).
      duration.microseconds = std::floor(duration.nanoseconds / 1000);
      // b. Set nanoseconds to nanoseconds modulo 1000.
      duration.nanoseconds = modulo(duration.nanoseconds, 1000);
      // c. Set milliseconds to floor(microseconds / 1000).
      duration.milliseconds = std::floor(duration.microseconds / 1000);
      // d. Set microseconds to microseconds modulo 1000.
      duration.microseconds = modulo(duration.microseconds, 1000);
      // e. Set seconds to floor(milliseconds / 1000).
      duration.seconds = std::floor(duration.milliseconds / 1000);
      // f. Set milliseconds to milliseconds modulo 1000.
      duration.milliseconds = modulo(duration.milliseconds, 1000);
      // g. Set minutes to floor(seconds, 60).
      duration.minutes = std::floor(duration.seconds / 60);
      // h. Set seconds to seconds modulo 60.
      duration.seconds = modulo(duration.seconds, 60);
      // i. Set hours to floor(minutes / 60).
      duration.hours = std::floor(duration.minutes / 60);
      // j. Set minutes to minutes modulo 60.
      duration.minutes = modulo(duration.minutes, 60);
      break;
    // 10. Else if largestUnit is "minute", then
    case Unit::kMinute:
      // a. Set microseconds to floor(nanoseconds / 1000).
      duration.microseconds = std::floor(duration.nanoseconds / 1000);
      // b. Set nanoseconds to nanoseconds modulo 1000.
      duration.nanoseconds = modulo(duration.nanoseconds, 1000);
      // c. Set milliseconds to floor(microseconds / 1000).
      duration.milliseconds = std::floor(duration.microseconds / 1000);
      // d. Set microseconds to microseconds modulo 1000.
      duration.microseconds = modulo(duration.microseconds, 1000);
      // e. Set seconds to floor(milliseconds / 1000).
      duration.seconds = std::floor(duration.milliseconds / 1000);
      // f. Set milliseconds to milliseconds modulo 1000.
      duration.milliseconds = modulo(duration.milliseconds, 1000);
      // g. Set minutes to floor(seconds / 60).
      duration.minutes = std::floor(duration.seconds / 60);
      // h. Set seconds to seconds modulo 60.
      duration.seconds = modulo(duration.seconds, 60);
      break;
    // 11. Else if largestUnit is "second", then
    case Unit::kSecond:
      // a. Set microseconds to floor(nanoseconds / 1000).
      duration.microseconds = std::floor(duration.nanoseconds / 1000);
      // b. Set nanoseconds to nanoseconds modulo 1000.
      duration.nanoseconds = modulo(duration.nanoseconds, 1000);
      // c. Set milliseconds to floor(microseconds / 1000).
      duration.milliseconds = std::floor(duration.microseconds / 1000);
      // d. Set microseconds to microseconds modulo 1000.
      duration.microseconds = modulo(duration.microseconds, 1000);
      // e. Set seconds to floor(milliseconds / 1000).
      duration.seconds = std::floor(duration.milliseconds / 1000);
      // f. Set milliseconds to milliseconds modulo 1000.
      duration.milliseconds = modulo(duration.milliseconds, 1000);
      break;
    // 12. Else if largestUnit is "millisecond", then
    case Unit::kMillisecond:
      // a. Set microseconds to floor(nanoseconds / 1000).
      duration.microseconds = std::floor(duration.nanoseconds / 1000);
      // b. Set nanoseconds to nanoseconds modulo 1000.
      duration.nanoseconds = modulo(duration.nanoseconds, 1000);
      // c. Set milliseconds to floor(microseconds / 1000).
      duration.milliseconds = std::floor(duration.microseconds / 1000);
      // d. Set microseconds to microseconds modulo 1000.
      duration.microseconds = modulo(duration.microseconds, 1000);
      break;
    // 13. Else if largestUnit is "microsecond", then
    case Unit::kMicrosecond:
      // a. Set microseconds to floor(nanoseconds / 1000).
      duration.microseconds = std::floor(duration.nanoseconds / 1000);
      // b. Set nanoseconds to nanoseconds modulo 1000.
      duration.nanoseconds = modulo(duration.nanoseconds, 1000);
      break;
    // 15. Else,
    case Unit::kNanosecond:
      // a. Assert: largestUnit is "nanosecond".
      break;
    case Unit::kAuto:
    case Unit::kNotPresent:
      UNREACHABLE();
  }
  // 15. Return ? CreateTimeDurationRecord(days, hours × sign, minutes × sign,
  // seconds × sign, milliseconds × sign, microseconds × sign, nanoseconds ×
  // sign).
  return TimeDurationRecord::Create(
      isolate, duration.days, duration.hours * sign, duration.minutes * sign,
      duration.seconds * sign, duration.milliseconds * sign,
      duration.microseconds * sign, duration.nanoseconds * sign);
}

// #sec-temporal-addinstant
MaybeHandle<BigInt> AddZonedDateTime(Isolate* isolate,
                                     Handle<BigInt> epoch_nanoseconds,
                                     Handle<JSReceiver> time_zone,
                                     Handle<JSReceiver> calendar,
                                     const DurationRecord& duration,
                                     const char* method_name) {
  TEMPORAL_ENTER_FUNC();

  // 1. If options is not present, set options to ! OrdinaryObjectCreate(null).
  Handle<JSReceiver> options = isolate->factory()->NewJSObjectWithNullProto();
  return AddZonedDateTime(isolate, epoch_nanoseconds, time_zone, calendar,
                          duration, options, method_name);
}

// #sec-temporal-addzoneddatetime
MaybeHandle<BigInt> AddZonedDateTime(Isolate* isolate,
                                     Handle<BigInt> epoch_nanoseconds,
                                     Handle<JSReceiver> time_zone,
                                     Handle<JSReceiver> calendar,
                                     const DurationRecord& duration,
                                     Handle<JSReceiver> options,
                                     const char* method_name) {
  TEMPORAL_ENTER_FUNC();

  TimeDurationRecord time_duration = duration.time_duration;
  // 2. If all of years, months, weeks, and days are 0, then
  if (duration.years == 0 && duration.months == 0 && duration.weeks == 0 &&
      time_duration.days == 0) {
    // a. Return ! AddInstant(epochNanoseconds, hours, minutes, seconds,
    // milliseconds, microseconds, nanoseconds).
    return AddInstant(isolate, epoch_nanoseconds, time_duration);
  }
  // 3. Let instant be ! CreateTemporalInstant(epochNanoseconds).
  Handle<JSTemporalInstant> instant;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, instant,
      temporal::CreateTemporalInstant(isolate, epoch_nanoseconds), BigInt);

  // 4. Let temporalDateTime be ?
  // BuiltinTimeZoneGetPlainDateTimeFor(timeZone, instant, calendar).
  Handle<JSTemporalPlainDateTime> temporal_date_time;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, temporal_date_time,
      temporal::BuiltinTimeZoneGetPlainDateTimeFor(isolate, time_zone, instant,
                                                   calendar, method_name),
      BigInt);
  // 5. Let datePart be ? CreateTemporalDate(temporalDateTime.[[ISOYear]],
  // temporalDateTime.[[ISOMonth]], temporalDateTime.[[ISODay]], calendar).
  Handle<JSTemporalPlainDate> date_part;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, date_part,
      CreateTemporalDate(
          isolate,
          {temporal_date_time->iso_year(), temporal_date_time->iso_month(),
           temporal_date_time->iso_day()},
          calendar),
      BigInt);
  // 6. Let dateDuration be ? CreateTemporalDuration(years, months, weeks, days,
  // 0, 0, 0, 0, 0, 0).
  Handle<JSTemporalDuration> date_duration;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, date_duration,
      CreateTemporalDuration(isolate, {duration.years,
                                       duration.months,
                                       duration.weeks,
                                       {time_duration.days, 0, 0, 0, 0, 0, 0}}),
      BigInt);
  // 7. Let addedDate be ? CalendarDateAdd(calendar, datePart, dateDuration,
  // options).
  Handle<JSTemporalPlainDate> added_date;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, added_date,
      CalendarDateAdd(isolate, calendar, date_part, date_duration, options),
      BigInt);
  // 8. Let intermediateDateTime be ?
  // CreateTemporalDateTime(addedDate.[[ISOYear]], addedDate.[[ISOMonth]],
  // addedDate.[[ISODay]], temporalDateTime.[[ISOHour]],
  // temporalDateTime.[[ISOMinute]], temporalDateTime.[[ISOSecond]],
  // temporalDateTime.[[ISOMillisecond]], temporalDateTime.[[ISOMicrosecond]],
  // temporalDateTime.[[ISONanosecond]], calendar).
  Handle<JSTemporalPlainDateTime> intermediate_date_time;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, intermediate_date_time,
      temporal::CreateTemporalDateTime(
          isolate,
          {{added_date->iso_year(), added_date->iso_month(),
            added_date->iso_day()},
           {temporal_date_time->iso_hour(), temporal_date_time->iso_minute(),
            temporal_date_time->iso_second(),
            temporal_date_time->iso_millisecond(),
            temporal_date_time->iso_microsecond(),
            temporal_date_time->iso_nanosecond()}},
          calendar),
      BigInt);
  // 9. Let intermediateInstant be ? BuiltinTimeZoneGetInstantFor(timeZone,
  // intermediateDateTime, "compatible").
  Handle<JSTemporalInstant> intermediate_instant;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, intermediate_instant,
      BuiltinTimeZoneGetInstantFor(isolate, time_zone, intermediate_date_time,
                                   Disambiguation::kCompatible, method_name),
      BigInt);
  // 10. Return ! AddInstant(intermediateInstant.[[Nanoseconds]], hours,
  // minutes, seconds, milliseconds, microseconds, nanoseconds).
  time_duration.days = 0;
  return AddInstant(
      isolate, Handle<BigInt>(intermediate_instant->nanoseconds(), isolate),
      time_duration);
}

// #sec-temporal-nanosecondstodays
Maybe<NanosecondsToDaysResult> NanosecondsToDays(Isolate* isolate,
                                                 double nanoseconds,
                                                 Handle<Object> relative_to_obj,
                                                 const char* method_name) {
  return NanosecondsToDays(isolate, BigInt::FromInt64(isolate, nanoseconds),
                           relative_to_obj, method_name);
}

Maybe<NanosecondsToDaysResult> NanosecondsToDays(Isolate* isolate,
                                                 Handle<BigInt> nanoseconds,
                                                 Handle<Object> relative_to_obj,
                                                 const char* method_name) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: Type(nanoseconds) is BigInt.
  // 2. Set nanoseconds to ℝ(nanoseconds).
  // 3. Let sign be ! ℝ(Sign(𝔽(nanoseconds))).
  ComparisonResult compare_result =
      BigInt::CompareToBigInt(nanoseconds, BigInt::FromInt64(isolate, 0));
  double sign = COMPARE_RESULT_TO_SIGN(compare_result);
  // 4. Let dayLengthNs be 8.64 × 10^13.
  Handle<BigInt> day_length_ns = BigInt::FromInt64(isolate, 86400000000000LLU);
  // 5. If sign is 0, then
  if (sign == 0) {
    // a. Return the new Record { [[Days]]: 0, [[Nanoseconds]]: 0,
    // [[DayLength]]: dayLengthNs }.
    NanosecondsToDaysResult result({0, 0, day_length_ns->AsInt64()});
    return Just(result);
  }
  // 6. If Type(relativeTo) is not Object or relativeTo does not have an
  // [[InitializedTemporalZonedDateTime]] internal slot, then
  if (!relative_to_obj->IsJSTemporalZonedDateTime()) {
    // Return the Record {
    // [[Days]]: the integral part of nanoseconds / dayLengthNs,
    // [[Nanoseconds]]: (abs(nanoseconds) modulo dayLengthNs) × sign,
    // [[DayLength]]: dayLengthNs }.
    Handle<BigInt> days_bigint;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, days_bigint,
        BigInt::Divide(isolate, nanoseconds, day_length_ns),
        Nothing<NanosecondsToDaysResult>());

    if (sign < 0) {
      nanoseconds = BigInt::UnaryMinus(isolate, nanoseconds);
    }

    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, nanoseconds,
        BigInt::Remainder(isolate, nanoseconds, day_length_ns),
        Nothing<NanosecondsToDaysResult>());
    NanosecondsToDaysResult result(
        {static_cast<double>(days_bigint->AsInt64()),
         static_cast<double>(nanoseconds->AsInt64() * sign),
         day_length_ns->AsInt64()});
    return Just(result);
  }
  Handle<JSTemporalZonedDateTime> relative_to =
      Handle<JSTemporalZonedDateTime>::cast(relative_to_obj);
  // 7. Let startNs be ℝ(relativeTo.[[Nanoseconds]]).
  Handle<BigInt> start_ns = Handle<BigInt>(relative_to->nanoseconds(), isolate);
  // 8. Let startInstant be ! CreateTemporalInstant(ℤ(sartNs)).
  Handle<JSTemporalInstant> start_instant;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, start_instant,
      temporal::CreateTemporalInstant(
          isolate, Handle<BigInt>(relative_to->nanoseconds(), isolate)),
      Nothing<NanosecondsToDaysResult>());

  // 9. Let startDateTime be ?
  // BuiltinTimeZoneGetPlainDateTimeFor(relativeTo.[[TimeZone]],
  // startInstant, relativeTo.[[Calendar]]).
  Handle<JSReceiver> time_zone =
      Handle<JSReceiver>(relative_to->time_zone(), isolate);
  Handle<JSReceiver> calendar =
      Handle<JSReceiver>(relative_to->calendar(), isolate);
  Handle<JSTemporalPlainDateTime> start_date_time;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, start_date_time,
      temporal::BuiltinTimeZoneGetPlainDateTimeFor(
          isolate, time_zone, start_instant, calendar, method_name),
      Nothing<NanosecondsToDaysResult>());

  // 10. Let endNs be startNs + nanoseconds.
  Handle<BigInt> end_ns;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, end_ns,
                                   BigInt::Add(isolate, start_ns, nanoseconds),
                                   Nothing<NanosecondsToDaysResult>());

  // 11. Let endInstant be ! CreateTemporalInstant(ℤ(endNs)).
  Handle<JSTemporalInstant> end_instant;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, end_instant, temporal::CreateTemporalInstant(isolate, end_ns),
      Nothing<NanosecondsToDaysResult>());
  // 12. Let endDateTime be ?
  // BuiltinTimeZoneGetPlainDateTimeFor(relativeTo.[[TimeZone]],
  // endInstant, relativeTo.[[Calendar]]).
  Handle<JSTemporalPlainDateTime> end_date_time;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, end_date_time,
      temporal::BuiltinTimeZoneGetPlainDateTimeFor(
          isolate, time_zone, end_instant, calendar, method_name),
      Nothing<NanosecondsToDaysResult>());

  // 13. Let dateDifference be ?
  // DifferenceISODateTime(startDateTime.[[ISOYear]],
  // startDateTime.[[ISOMonth]], startDateTime.[[ISODay]],
  // startDateTime.[[ISOHour]], startDateTime.[[ISOMinute]],
  // startDateTime.[[ISOSecond]], startDateTime.[[ISOMillisecond]],
  // startDateTime.[[ISOMicrosecond]], startDateTime.[[ISONanosecond]],
  // endDateTime.[[ISOYear]], endDateTime.[[ISOMonth]], endDateTime.[[ISODay]],
  // endDateTime.[[ISOHour]], endDateTime.[[ISOMinute]],
  // endDateTime.[[ISOSecond]], endDateTime.[[ISOMillisecond]],
  // endDateTime.[[ISOMicrosecond]], endDateTime.[[ISONanosecond]],
  // relativeTo.[[Calendar]], "day").
  DurationRecord date_difference;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, date_difference,
      DifferenceISODateTime(
          isolate,
          {{start_date_time->iso_year(), start_date_time->iso_month(),
            start_date_time->iso_day()},
           {start_date_time->iso_hour(), start_date_time->iso_minute(),
            start_date_time->iso_second(), start_date_time->iso_millisecond(),
            start_date_time->iso_microsecond(),
            start_date_time->iso_nanosecond()}},
          {{end_date_time->iso_year(), end_date_time->iso_month(),
            end_date_time->iso_day()},
           {end_date_time->iso_hour(), end_date_time->iso_minute(),
            end_date_time->iso_second(), end_date_time->iso_millisecond(),
            end_date_time->iso_microsecond(), end_date_time->iso_nanosecond()}},
          calendar, Unit::kDay, relative_to, method_name),
      Nothing<NanosecondsToDaysResult>());

  // 14. Let days be dateDifference.[[Days]].
  double days = date_difference.time_duration.days;

  // 15. Let intermediateNs be ℝ(? AddZonedDateTime(ℤ(startNs),
  // relativeTo.[[TimeZone]], relativeTo.[[Calendar]], 0, 0, 0, days, 0, 0, 0,
  // 0, 0, 0)).
  Handle<BigInt> intermediate_ns;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, intermediate_ns,
      AddZonedDateTime(isolate, start_ns, time_zone, calendar,
                       {0, 0, 0, {days, 0, 0, 0, 0, 0, 0}}, method_name),
      Nothing<NanosecondsToDaysResult>());

  // 16. If sign is 1, then
  if (sign == 1) {
    // a. Repeat, while days > 0 and intermediateNs > endNs,
    while (days > 0 && BigInt::CompareToBigInt(intermediate_ns, end_ns) ==
                           ComparisonResult::kGreaterThan) {
      // i. Set days to days − 1.
      days -= 1;
      // ii. Set intermediateNs to ℝ(? AddZonedDateTime(ℤ(startNs),
      // relativeTo.[[TimeZone]], relativeTo.[[Calendar]], 0, 0, 0, days, 0, 0,
      // 0, 0, 0, 0)).
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, intermediate_ns,
          AddZonedDateTime(isolate, start_ns, time_zone, calendar,
                           {0, 0, 0, {days, 0, 0, 0, 0, 0, 0}}, method_name),
          Nothing<NanosecondsToDaysResult>());
    }
  }

  // 17. Set nanoseconds to endNs − intermediateNs.
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, nanoseconds, BigInt::Subtract(isolate, end_ns, intermediate_ns),
      Nothing<NanosecondsToDaysResult>());

  // 18. Let done be false.
  bool done = false;

  // 19. Repeat, while done is false,
  while (!done) {
    // a. Let oneDayFartherNs be ℝ(? AddZonedDateTime(ℤ(intermediateNs),
    // relativeTo.[[TimeZone]], relativeTo.[[Calendar]], 0, 0, 0, sign, 0, 0, 0,
    // 0, 0, 0)).
    Handle<BigInt> one_day_farther_ns;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, one_day_farther_ns,
        AddZonedDateTime(isolate, intermediate_ns, time_zone, calendar,
                         {0, 0, 0, {sign, 0, 0, 0, 0, 0, 0}}, method_name),
        Nothing<NanosecondsToDaysResult>());

    // b. Set dayLengthNs to oneDayFartherNs − intermediateNs.
    Handle<BigInt> day_length_ns;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, day_length_ns,
        BigInt::Subtract(isolate, one_day_farther_ns, intermediate_ns),
        Nothing<NanosecondsToDaysResult>());

    // c. If (nanoseconds − dayLengthNs) × sign ≥ 0, then
    compare_result = BigInt::CompareToBigInt(nanoseconds, day_length_ns);
    if (sign * COMPARE_RESULT_TO_SIGN(compare_result) >= 0) {
      // i. Set nanoseconds to nanoseconds − dayLengthNs.
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, nanoseconds,
          BigInt::Subtract(isolate, nanoseconds, day_length_ns),
          Nothing<NanosecondsToDaysResult>());

      // ii. Set intermediateNs to oneDayFartherNs.
      intermediate_ns = one_day_farther_ns;

      // iii. Set days to days + sign.
      days += sign;
      // d. Else,
    } else {
      // i. Set done to true.
      done = true;
    }
  }

  // 20. Return the new Record { [[Days]]: days, [[Nanoseconds]]: nanoseconds,
  // [[DayLength]]: abs(dayLengthNs) }.
  NanosecondsToDaysResult result({days,
                                  static_cast<double>(nanoseconds->AsInt64()),
                                  std::abs(day_length_ns->AsInt64())});
  return Just(result);
}

Maybe<DurationRecord> DifferenceISODateTime(
    Isolate* isolate, const DateTimeRecordCommon& date_time1,
    const DateTimeRecordCommon& date_time2, Handle<JSReceiver> calendar,
    Unit largest_unit, Handle<Object> options_obj, const char* method_name) {
  TEMPORAL_ENTER_FUNC();

  Factory* factory = isolate->factory();
  DurationRecord result;
  // 1. Assert: y1, mon1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2, d2, h2,
  // min2, s2, ms2, mus2, and ns2 are integers.
  // 2. If options is not present, set options to ! OrdinaryObjectCreate(null).
  Handle<JSReceiver> options;
  if (options_obj->IsUndefined()) {
    options = factory->NewJSObjectWithNullProto();
  } else {
    DCHECK(options_obj->IsJSReceiver());
    options = Handle<JSReceiver>::cast(options_obj);
  }
  // 3. Let timeDifference be ! DifferenceTime(h1, min1, s1, ms1, mus1, ns1, h2,
  // min2, s2, ms2, mus2, ns2).
  TimeDurationRecord time_difference;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, time_difference,
      DifferenceTime(isolate, date_time1.time, date_time2.time),
      Nothing<DurationRecord>());

  result.time_duration = time_difference;
  result.time_duration.days = 0;

  // 4. Let timeSign be ! DurationSign(0, 0, 0, timeDifference.[[Days]],
  // timeDifference.[[Hours]], timeDifference.[[Minutes]],
  // timeDifference.[[Seconds]], timeDifference.[[Milliseconds]],
  // timeDifference.[[Microseconds]], timeDifference.[[Nanoseconds]]).
  double time_sign = DurationSign(isolate, {0, 0, 0, time_difference});
  // 5. Let dateSign be ! CompareISODate(y2, mon2, d2, y1, mon1, d1).
  double date_sign = CompareISODate(isolate, date_time2.date, date_time1.date);
  // 6. Let balanceResult be ! BalanceISODate(y1, mon1, d1 +
  // timeDifference.[[Days]]).
  DateRecordCommon balanced = BalanceISODate(
      isolate,
      {date_time1.date.year, date_time1.date.month,
       date_time1.date.day + static_cast<int32_t>(time_difference.days)});

  // 7. If timeSign is -dateSign, then
  if (time_sign == -date_sign) {
    // a. Set balanceResult be ! BalanceISODate(balanceResult.[[Year]],
    // balanceResult.[[Month]], balanceResult.[[Day]] - timeSign).
    balanced.day -= time_sign;
    balanced = BalanceISODate(isolate, balanced);
    // b. Set timeDifference to ? BalanceDuration(-timeSign,
    // timeDifference.[[Hours]], timeDifference.[[Minutes]],
    // timeDifference.[[Seconds]], timeDifference.[[Milliseconds]],
    // timeDifference.[[Microseconds]], timeDifference.[[Nanoseconds]],
    // largestUnit).

    MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, result.time_duration,
        BalanceDuration(
            isolate, largest_unit,
            {-time_sign, time_difference.hours, time_difference.minutes,
             time_difference.seconds, time_difference.milliseconds,
             time_difference.microseconds, time_difference.nanoseconds},
            method_name),
        Nothing<DurationRecord>());
  }
  // 8. Let date1 be ? CreateTemporalDate(balanceResult.[[Year]],
  // balanceResult.[[Month]], balanceResult.[[Day]], calendar).
  Handle<JSTemporalPlainDate> date1;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, date1, CreateTemporalDate(isolate, balanced, calendar),
      Nothing<DurationRecord>());
  // 9. Let date2 be ? CreateTemporalDate(y2, mon2, d2, calendar).
  Handle<JSTemporalPlainDate> date2;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, date2, CreateTemporalDate(isolate, date_time2.date, calendar),
      Nothing<DurationRecord>());
  // 10. Let dateLargestUnit be ! LargerOfTwoTemporalUnits("day", largestUnit).
  Unit date_largest_unit =
      LargerOfTwoTemporalUnits(isolate, Unit::kDay, largest_unit);

  // 11. Let untilOptions be ? MergeLargestUnitOption(options, dateLargestUnit).
  Handle<JSObject> until_options;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, until_options,
      MergeLargestUnitOption(isolate, options, date_largest_unit),
      Nothing<DurationRecord>());
  // 12. Let dateDifference be ? CalendarDateUntil(calendar, date1, date2,
  // untilOptions).
  Handle<JSTemporalDuration> date_difference;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, date_difference,
      CalendarDateUntil(isolate, calendar, date1, date2, until_options),
      Nothing<DurationRecord>());
  // 13. Let balanceResult be ? BalanceDuration(dateDifference.[[Days]],
  // timeDifference.[[Hours]], timeDifference.[[Minutes]],
  // timeDifference.[[Seconds]], timeDifference.[[Milliseconds]],
  // timeDifference.[[Microseconds]], timeDifference.[[Nanoseconds]],
  // largestUnit).

  TimeDurationRecord balance_result;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, balance_result,
      BalanceDuration(
          isolate, largest_unit,
          {date_difference->days().Number(), time_difference.hours,
           time_difference.minutes, time_difference.seconds,
           time_difference.milliseconds, time_difference.microseconds,
           time_difference.nanoseconds},
          method_name),
      Nothing<DurationRecord>());

  result.time_duration = balance_result;
  // 14. Return the Record { [[Years]]: dateDifference.[[Years]], [[Months]]:
  // dateDifference.[[Months]], [[Weeks]]: dateDifference.[[Weeks]], [[Days]]:
  // balanceResult.[[Days]], [[Hours]]: balanceResult.[[Hours]], [[Minutes]]:
  // balanceResult.[[Minutes]], [[Seconds]]: balanceResult.[[Seconds]],
  // [[Milliseconds]]: balanceResult.[[Milliseconds]], [[Microseconds]]:
  // balanceResult.[[Microseconds]], [[Nanoseconds]]:
  // balanceResult.[[Nanoseconds]] }.
  result.years = date_difference->years().Number();
  result.months = date_difference->months().Number();
  result.weeks = date_difference->weeks().Number();
  return Just(result);
}

// #sec-temporal-addinstant
MaybeHandle<BigInt> AddInstant(Isolate* isolate,
                               Handle<BigInt> epoch_nanoseconds,
                               const TimeDurationRecord& addend) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: hours, minutes, seconds, milliseconds, microseconds, and
  // nanoseconds are integer Number values.
  // 2. Let result be epochNanoseconds + ℤ(nanoseconds) +
  // ℤ(microseconds) × 1000ℤ + ℤ(milliseconds) × 10^6ℤ + ℤ(seconds) × 10^9ℤ +
  // ℤ(minutes) × 60ℤ × 10^9ℤ + ℤ(hours) × 3600ℤ × 10^9ℤ.
  Handle<BigInt> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      BigInt::Add(isolate, epoch_nanoseconds,
                  BigInt::FromInt64(isolate, addend.nanoseconds)),
      BigInt);
  Handle<BigInt> temp;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, temp,
      BigInt::Multiply(isolate, BigInt::FromInt64(isolate, addend.microseconds),
                       BigInt::FromInt64(isolate, 1000)),
      BigInt);
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                             BigInt::Add(isolate, result, temp), BigInt);

  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, temp,
      BigInt::Multiply(isolate, BigInt::FromInt64(isolate, addend.milliseconds),
                       BigInt::FromInt64(isolate, 1000000)),
      BigInt);
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                             BigInt::Add(isolate, result, temp), BigInt);

  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, temp,
      BigInt::Multiply(isolate, BigInt::FromInt64(isolate, addend.seconds),
                       BigInt::FromInt64(isolate, 1000000000)),
      BigInt);
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                             BigInt::Add(isolate, result, temp), BigInt);

  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, temp,
      BigInt::Multiply(isolate, BigInt::FromInt64(isolate, addend.minutes),
                       BigInt::FromInt64(isolate, 1000000000)),
      BigInt);
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, temp,
      BigInt::Multiply(isolate, temp, BigInt::FromInt64(isolate, 60)), BigInt);
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                             BigInt::Add(isolate, result, temp), BigInt);

  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, temp,
      BigInt::Multiply(isolate, BigInt::FromInt64(isolate, addend.hours),
                       BigInt::FromInt64(isolate, 1000000000)),
      BigInt);
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, temp,
      BigInt::Multiply(isolate, temp, BigInt::FromInt64(isolate, 3600)),
      BigInt);
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                             BigInt::Add(isolate, result, temp), BigInt);

  // 3. If ! IsValidEpochNanoseconds(result) is false, throw a RangeError
  // exception.
  if (!IsValidEpochNanoseconds(isolate, result)) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(), BigInt);
  }
  // 4. Return result.
  return result;
}

// #sec-temporal-isvalidepochnanoseconds
bool IsValidEpochNanoseconds(Isolate* isolate,
                             Handle<BigInt> epoch_nanoseconds) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: Type(epochNanoseconds) is BigInt.
  // 2. If epochNanoseconds < −86400ℤ × 10^17ℤ or epochNanoseconds > 86400ℤ ×
  // 10^17ℤ, then a. Return false.
  // 3. Return true.
  int64_t ns = epoch_nanoseconds->AsInt64();
  return !(ns < -86400 * 1e17 || ns > 86400 * 1e17);
}

Handle<BigInt> GetEpochFromISOParts(Isolate* isolate,
                                    const DateTimeRecordCommon& date_time) {
  TEMPORAL_ENTER_FUNC();
  // 1. Assert: year, month, day, hour, minute, second, millisecond,
  // microsecond, and nanosecond are integers.
  // 2. Assert: ! IsValidISODate(year, month, day) is true.
  DCHECK(IsValidISODate(isolate, date_time.date));
  // 3. Assert: ! IsValidTime(hour, minute, second, millisecond, microsecond,
  // nanosecond) is true.
  DCHECK(IsValidTime(isolate, date_time.time));
  // 4. Let date be ! MakeDay(𝔽(year), 𝔽(month − 1), 𝔽(day)).
  double date = MakeDay(date_time.date.year, date_time.date.month - 1,
                        date_time.date.day);
  // 5. Let time be ! MakeTime(𝔽(hour), 𝔽(minute), 𝔽(second), 𝔽(millisecond)).
  double time = MakeTime(date_time.time.hour, date_time.time.minute,
                         date_time.time.second, date_time.time.millisecond);
  // 6. Let ms be ! MakeDate(date, time).
  double ms = MakeDate(date, time);
  // 7. Assert: ms is finite.
  // 8. Return ℝ(ms) × 10^6 + microsecond × 10^3 + nanosecond.
  return BigInt::Add(
             isolate,
             BigInt::Add(
                 isolate,
                 BigInt::Multiply(
                     isolate,
                     BigInt::FromNumber(isolate,
                                        isolate->factory()->NewNumber(ms))
                         .ToHandleChecked(),
                     BigInt::FromInt64(isolate, 1000000))
                     .ToHandleChecked(),
                 BigInt::Multiply(
                     isolate,
                     BigInt::FromInt64(isolate, date_time.time.microsecond),
                     BigInt::FromInt64(isolate, 1000))
                     .ToHandleChecked())
                 .ToHandleChecked(),
             BigInt::FromInt64(isolate, date_time.time.nanosecond))
      .ToHandleChecked();
}

// #sec-temporal-durationsign
int32_t DurationSign(Isolate* isolaet, const DurationRecord& dur) {
  TEMPORAL_ENTER_FUNC();

  // 1. For each value v of « years, months, weeks, days, hours, minutes,
  // seconds, milliseconds, microseconds, nanoseconds », do a. If v < 0, return
  // −1. b. If v > 0, return 1.
  // 2. Return 0.
  if (dur.years < 0) return -1;
  if (dur.years > 0) return 1;
  if (dur.months < 0) return -1;
  if (dur.months > 0) return 1;
  if (dur.weeks < 0) return -1;
  if (dur.weeks > 0) return 1;
  const TimeDurationRecord& time = dur.time_duration;
  if (time.days < 0) return -1;
  if (time.days > 0) return 1;
  if (time.hours < 0) return -1;
  if (time.hours > 0) return 1;
  if (time.minutes < 0) return -1;
  if (time.minutes > 0) return 1;
  if (time.seconds < 0) return -1;
  if (time.seconds > 0) return 1;
  if (time.milliseconds < 0) return -1;
  if (time.milliseconds > 0) return 1;
  if (time.microseconds < 0) return -1;
  if (time.microseconds > 0) return 1;
  if (time.nanoseconds < 0) return -1;
  if (time.nanoseconds > 0) return 1;
  return 0;
}

// #sec-temporal-isvalidduration
bool IsValidDuration(Isolate* isolate, const DurationRecord& dur) {
  TEMPORAL_ENTER_FUNC();

  // 1. Let sign be ! DurationSign(years, months, weeks, days, hours, minutes,
  // seconds, milliseconds, microseconds, nanoseconds).
  int32_t sign = DurationSign(isolate, dur);
  // 2. For each value v of « years, months, weeks, days, hours, minutes,
  // seconds, milliseconds, microseconds, nanoseconds », do a. If v is not
  // finite, return false. b. If v < 0 and sign > 0, return false. c. If v > 0
  // and sign < 0, return false.
  // 3. Return true.
  const TimeDurationRecord& time = dur.time_duration;
  return !((sign > 0 && (dur.years < 0 || dur.months < 0 || dur.weeks < 0 ||
                         time.days < 0 || time.hours < 0 || time.minutes < 0 ||
                         time.seconds < 0 || time.milliseconds < 0 ||
                         time.microseconds < 0 || time.nanoseconds < 0)) ||
           (sign < 0 && (dur.years > 0 || dur.months > 0 || dur.weeks > 0 ||
                         time.days > 0 || time.hours > 0 || time.minutes > 0 ||
                         time.seconds > 0 || time.milliseconds > 0 ||
                         time.microseconds > 0 || time.nanoseconds > 0)));
}

// #sec-temporal-isisoleapyear
bool IsISOLeapYear(Isolate* isolate, int32_t year) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: year is an integer.
  // 2. If year modulo 4 ≠ 0, return false.
  // 3. If year modulo 400 = 0, return true.
  // 4. If year modulo 100 = 0, return false.
  // 5. Return true.
  return isolate->date_cache()->IsLeap(year);
}

// #sec-temporal-isodaysinmonth
int32_t ISODaysInMonth(Isolate* isolate, int32_t year, int32_t month) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: year is an integer.
  // 2. Assert: month is an integer, month ≥ 1, and month ≤ 12.
  DCHECK_GE(month, 1);
  DCHECK_LE(month, 12);
  // 3. If month is 1, 3, 5, 7, 8, 10, or 12, return 31.
  if (month % 2 == ((month < 8) ? 1 : 0)) return 31;
  // 4. If month is 4, 6, 9, or 11, return 30.
  DCHECK(month == 2 || month == 4 || month == 6 || month == 9 || month == 11);
  if (month != 2) return 30;
  // 5. If ! IsISOLeapYear(year) is true, return 29.
  return IsISOLeapYear(isolate, year) ? 29 : 28;
  // 6. Return 28.
}

// #sec-temporal-isodaysinyear
int32_t ISODaysInYear(Isolate* isolate, int32_t year) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: year is an integer.
  // 2. If ! IsISOLeapYear(year) is true, then
  // a. Return 366.
  // 3. Return 365.
  return IsISOLeapYear(isolate, year) ? 366 : 365;
}

bool IsValidTime(Isolate* isolate, const TimeRecordCommon& time) {
  TEMPORAL_ENTER_FUNC();

  // 2. If hour < 0 or hour > 23, then
  // a. Return false.
  if (time.hour < 0 || time.hour > 23) return false;
  // 3. If minute < 0 or minute > 59, then
  // a. Return false.
  if (time.minute < 0 || time.minute > 59) return false;
  // 4. If second < 0 or second > 59, then
  // a. Return false.
  if (time.second < 0 || time.second > 59) return false;
  // 5. If millisecond < 0 or millisecond > 999, then
  // a. Return false.
  if (time.millisecond < 0 || time.millisecond > 999) return false;
  // 6. If microsecond < 0 or microsecond > 999, then
  // a. Return false.
  if (time.microsecond < 0 || time.microsecond > 999) return false;
  // 7. If nanosecond < 0 or nanosecond > 999, then
  // a. Return false.
  if (time.nanosecond < 0 || time.nanosecond > 999) return false;
  // 8. Return true.
  return true;
}

// #sec-temporal-isvalidisodate
bool IsValidISODate(Isolate* isolate, const DateRecordCommon& date) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: year, month, and day are integers.
  // 2. If month < 1 or month > 12, then
  // a. Return false.
  if (date.month < 1 || date.month > 12) return false;
  // 3. Let daysInMonth be ! ISODaysInMonth(year, month).
  // 4. If day < 1 or day > daysInMonth, then
  // a. Return false.
  if (date.day < 1 ||
      date.day > ISODaysInMonth(isolate, date.year, date.month)) {
    return false;
  }
  // 5. Return true.
  return true;
}

// #sec-temporal-compareisodate
int32_t CompareISODate(Isolate* isolate, const DateRecordCommon& date1,
                       const DateRecordCommon& date2) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: y1, m1, d1, y2, m2, and d2 are integers.
  // 2. If y1 > y2, return 1.
  if (date1.year > date2.year) return 1;
  // 3. If y1 < y2, return -1.
  if (date1.year < date2.year) return -1;
  // 4. If m1 > m2, return 1.
  if (date1.month > date2.month) return 1;
  // 5. If m1 < m2, return -1.
  if (date1.month < date2.month) return -1;
  // 6. If d1 > d2, return 1.
  if (date1.day > date2.day) return 1;
  // 7. If d1 < d2, return -1.
  if (date1.day < date2.day) return -1;
  // 8. Return 0.
  return 0;
}

inline int32_t floor_divid(int32_t a, int32_t b) {
  return (((a) / (b)) + ((((a) < 0) && (((a) % (b)) != 0)) ? -1 : 0));
}
// #sec-temporal-balanceisoyearmonth
void BalanceISOYearMonth(Isolate* isolate, int32_t* year, int32_t* month) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: year and month are integers.
  // 2. Set year to year + floor((month - 1) / 12).
  *year += floor_divid((*month - 1), 12);
  // 3. Set month to (month − 1) modulo 12 + 1.
  *month = static_cast<int32_t>(modulo(*month - 1, 12)) + 1;

  // 4. Return the new Record { [[Year]]: year, [[Month]]: month }.
}
// #sec-temporal-balancetime
DateTimeRecordCommon BalanceTime(const TimeRecordCommon& input) {
  TEMPORAL_ENTER_FUNC();
  TimeRecordCommon time(input);

  // 1. Assert: hour, minute, second, millisecond, microsecond, and nanosecond
  // are integers.
  // 2. Set microsecond to microsecond + floor(nanosecond / 1000).
  time.microsecond += std::floor(time.nanosecond / 1000);
  // 3. Set nanosecond to nanosecond modulo 1000.
  time.nanosecond = modulo(time.nanosecond, 1000);
  // 4. Set millisecond to millisecond + floor(microsecond / 1000).
  time.millisecond += std::floor(time.microsecond / 1000);
  // 5. Set microsecond to microsecond modulo 1000.
  time.microsecond = modulo(time.microsecond, 1000);
  // 6. Set second to second + floor(millisecond / 1000).
  time.second += std::floor(time.millisecond / 1000);
  // 7. Set millisecond to millisecond modulo 1000.
  time.millisecond = modulo(time.millisecond, 1000);
  // 8. Set minute to minute + floor(second / 60).
  time.minute += std::floor(time.second / 60);
  // 9. Set second to second modulo 60.
  time.second = modulo(time.second, 60);
  // 10. Set hour to hour + floor(minute / 60).
  time.hour += std::floor(time.minute / 60);
  // 11. Set minute to minute modulo 60.
  time.minute = modulo(time.minute, 60);
  // 12. Let days be floor(hour / 24).
  DateRecordCommon date = {0, 0,
                           static_cast<int32_t>(std::floor(time.hour / 24))};
  // 13. Set hour to hour modulo 24.
  time.hour = modulo(time.hour, 24);
  // 14. Return the new Record { [[Days]]: days, [[Hour]]: hour, [[Minute]]:
  // minute, [[Second]]: second, [[Millisecond]]: millisecond, [[Microsecond]]:
  // microsecond, [[Nanosecond]]: nanosecond }.
  return {date, time};
}

// #sec-temporal-differencetime
Maybe<TimeDurationRecord> DifferenceTime(Isolate* isolate,
                                         const TimeRecordCommon& time1,
                                         const TimeRecordCommon& time2) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: h1, min1, s1, ms1, mus1, ns1, h2, min2, s2, ms2, mus2, and ns2
  // are integers.
  TimeDurationRecord dur;
  // 2. Let hours be h2 − h1.
  dur.hours = time2.hour - time1.hour;
  // 3. Let minutes be min2 − min1.
  dur.minutes = time2.minute - time1.minute;
  // 4. Let seconds be s2 − s1.
  dur.seconds = time2.second - time1.second;
  // 5. Let milliseconds be ms2 − ms1.
  dur.milliseconds = time2.millisecond - time1.millisecond;
  // 6. Let microseconds be mus2 − mus1.
  dur.microseconds = time2.microsecond - time1.microsecond;
  // 7. Let nanoseconds be ns2 − ns1.
  dur.nanoseconds = time2.nanosecond - time1.nanosecond;
  // 8. Let sign be ! DurationSign(0, 0, 0, 0, hours, minutes, seconds,
  // milliseconds, microseconds, nanoseconds).
  double sign = DurationSign(
      isolate, {0,
                0,
                0,
                {0, dur.hours, dur.minutes, dur.seconds, dur.milliseconds,
                 dur.microseconds, dur.nanoseconds}});

  // 9. Let bt be ! BalanceTime(hours × sign, minutes × sign, seconds × sign,
  // milliseconds × sign, microseconds × sign, nanoseconds × sign).
  DateTimeRecordCommon bt =
      BalanceTime({static_cast<int32_t>(dur.hours * sign),
                   static_cast<int32_t>(dur.minutes * sign),
                   static_cast<int32_t>(dur.seconds * sign),
                   static_cast<int32_t>(dur.milliseconds * sign),
                   static_cast<int32_t>(dur.microseconds * sign),
                   static_cast<int32_t>(dur.nanoseconds * sign)});

  // 9. Return ! CreateTimeDurationRecord(bt.[[Days]] × sign, bt.[[Hour]] ×
  // sign, bt.[[Minute]] × sign, bt.[[Second]] × sign, bt.[[Millisecond]] ×
  // sign, bt.[[Microsecond]] × sign, bt.[[Nanosecond]] × sign).
  return TimeDurationRecord::Create(
      isolate, bt.date.day * sign, bt.time.hour * sign, bt.time.minute * sign,
      bt.time.second * sign, bt.time.millisecond * sign,
      bt.time.microsecond * sign, bt.time.nanosecond * sign);
}

// #sec-temporal-addtime
DateTimeRecordCommon AddTime(Isolate* isolate, const TimeRecordCommon& time,
                             const TimeDurationRecord& addend) {
  TEMPORAL_ENTER_FUNC();

  DCHECK_EQ(addend.days, 0);
  // 1. Assert: hour, minute, second, millisecond, microsecond, nanosecond,
  // hours, minutes, seconds, milliseconds, microseconds, and nanoseconds are
  // integers.
  // 2. Let hour be hour + hours.
  return BalanceTime(
      {time.hour + static_cast<int32_t>(addend.hours),
       // 3. Let minute be minute + minutes.
       time.minute + static_cast<int32_t>(addend.minutes),
       // 4. Let second be second + seconds.
       time.second + static_cast<int32_t>(addend.seconds),
       // 5. Let millisecond be millisecond + milliseconds.
       time.millisecond + static_cast<int32_t>(addend.milliseconds),
       // 6. Let microsecond be microsecond + microseconds.
       time.microsecond + static_cast<int32_t>(addend.microseconds),
       // 7. Let nanosecond be nanosecond + nanoseconds.
       time.nanosecond + static_cast<int32_t>(addend.nanoseconds)});
  // 8. Return ! BalanceTime(hour, minute, second, millisecond, microsecond,
  // nanosecond).
}

// #sec-temporal-totaldurationnanoseconds
double TotalDurationNanoseconds(Isolate* isolate,
                                const TimeDurationRecord& value,
                                double offset_shift) {
  TEMPORAL_ENTER_FUNC();

  TimeDurationRecord duration(value);

  // 1. Assert: offsetShift is an integer.
  // 2. Set nanoseconds to ℝ(nanoseconds).
  // 3. If days ≠ 0, then
  if (duration.days != 0) {
    // a. Set nanoseconds to nanoseconds − offsetShift.
    duration.nanoseconds -= offset_shift;
  }

  // 4. Set hours to ℝ(hours) + ℝ(days) × 24.
  duration.hours += duration.days * 24;

  // 5. Set minutes to ℝ(minutes) + hours × 60.
  duration.minutes += duration.hours * 60;

  // 6. Set seconds to ℝ(seconds) + minutes × 60.
  duration.seconds += duration.minutes * 60;

  // 7. Set milliseconds to ℝ(milliseconds) + seconds × 1000.
  duration.milliseconds += duration.seconds * 1000;

  // 8. Set microseconds to ℝ(microseconds) + milliseconds × 1000.
  duration.microseconds += duration.milliseconds * 1000;

  // 9. Return nanoseconds + microseconds × 1000.
  return duration.nanoseconds + duration.microseconds * 1000;
}

Maybe<DateRecordCommon> RegulateISODate(Isolate* isolate, ShowOverflow overflow,
                                        const DateRecordCommon& date);
Maybe<int32_t> ResolveISOMonth(Isolate* isolate, Handle<JSReceiver> fields);

// #sec-temporal-isomonthdayfromfields
Maybe<DateRecordCommon> ISOMonthDayFromFields(Isolate* isolate,
                                              Handle<JSReceiver> fields,
                                              Handle<JSReceiver> options,
                                              const char* method_name) {
  Factory* factory = isolate->factory();
  // 1. Assert: Type(fields) is Object.
  // 2. Let overflow be ? ToTemporalOverflow(options).
  ShowOverflow overflow;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, overflow, ToTemporalOverflow(isolate, options, method_name),
      Nothing<DateRecordCommon>());

  // 3. Set fields to ? PrepareTemporalFields(fields, « "day", "month",
  // "monthCode", "year" », «»).
  Handle<FixedArray> field_names = factory->NewFixedArray(4);
  field_names->set(0, ReadOnlyRoots(isolate).day_string());
  field_names->set(1, ReadOnlyRoots(isolate).month_string());
  field_names->set(2, ReadOnlyRoots(isolate).monthCode_string());
  field_names->set(3, ReadOnlyRoots(isolate).year_string());
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, fields,
      PrepareTemporalFields(isolate, fields, field_names,
                            RequiredFields::kNone),
      Nothing<DateRecordCommon>());
  // 4. Let month be ! Get(fields, "month").
  Handle<Object> month_obj =
      Object::GetPropertyOrElement(isolate, fields, factory->month_string())
          .ToHandleChecked();
  // 5. Let monthCode be ! Get(fields, "monthCode").
  Handle<Object> month_code_obj =
      Object::GetPropertyOrElement(isolate, fields, factory->monthCode_string())
          .ToHandleChecked();
  // 6. Let year be ! Get(fields, "year").
  Handle<Object> year_obj =
      Object::GetPropertyOrElement(isolate, fields, factory->year_string())
          .ToHandleChecked();
  // 7. If month is not undefined, and monthCode and year are both undefined,
  // then
  if (!month_obj->IsUndefined(isolate) &&
      month_code_obj->IsUndefined(isolate) && year_obj->IsUndefined(isolate)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR_RETURN_VALUE(isolate, NEW_TEMPORAL_INVALID_ARG_TYPE_ERROR(),
                                 Nothing<DateRecordCommon>());
  }
  // 8. Set month to ? ResolveISOMonth(fields).
  DateRecordCommon result;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, result.month,
                                         ResolveISOMonth(isolate, fields),
                                         Nothing<DateRecordCommon>());

  // 9. Let day be ! Get(fields, "day").
  Handle<Object> day_obj =
      Object::GetPropertyOrElement(isolate, fields, factory->day_string())
          .ToHandleChecked();
  // 10. If day is undefined, throw a TypeError exception.
  if (day_obj->IsUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_VALUE(isolate, NEW_TEMPORAL_INVALID_ARG_TYPE_ERROR(),
                                 Nothing<DateRecordCommon>());
  }
  result.day = FastD2I(floor(day_obj->Number()));
  // 11. Let referenceISOYear be 1972 (the first leap year after the Unix
  // epoch).
  int32_t reference_iso_year = 1972;
  // 12. If monthCode is undefined, then
  if (month_code_obj->IsUndefined(isolate)) {
    result.year = FastD2I(floor(year_obj->Number()));
    // a. Let result be ? RegulateISODate(year, month, day, overflow).
  } else {
    // 13. Else,
    // a. Let result be ? RegulateISODate(referenceISOYear, month, day,
    // overflow).
    result.year = reference_iso_year;
  }
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, result, RegulateISODate(isolate, overflow, result),
      Nothing<DateRecordCommon>());
  // 14. Return the new Record { [[Month]]: result.[[Month]], [[Day]]:
  // result.[[Day]], [[ReferenceISOYear]]: referenceISOYear }.
  result.year = reference_iso_year;
  return Just(result);
}

}  // namespace

// #sec-temporal.duration
MaybeHandle<JSTemporalDuration> JSTemporalDuration::Constructor(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    Handle<Object> years, Handle<Object> months, Handle<Object> weeks,
    Handle<Object> days, Handle<Object> hours, Handle<Object> minutes,
    Handle<Object> seconds, Handle<Object> milliseconds,
    Handle<Object> microseconds, Handle<Object> nanoseconds) {
  const char* method_name = "Temporal.Duration";
  // 1. If NewTarget is undefined, then
  if (new_target->IsUndefined()) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     method_name)),
                    JSTemporalDuration);
  }
  // 2. Let y be ? ToIntegerWithoutRounding(years).
  double y;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, y, ToIntegerWithoutRounding(isolate, years),
      Handle<JSTemporalDuration>());

  // 3. Let mo be ? ToIntegerWithoutRounding(months).
  double mo;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, mo, ToIntegerWithoutRounding(isolate, months),
      Handle<JSTemporalDuration>());

  // 4. Let w be ? ToIntegerWithoutRounding(weeks).
  double w;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, w, ToIntegerWithoutRounding(isolate, weeks),
      Handle<JSTemporalDuration>());

  // 5. Let d be ? ToIntegerWithoutRounding(days).
  double d;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, d, ToIntegerWithoutRounding(isolate, days),
      Handle<JSTemporalDuration>());

  // 6. Let h be ? ToIntegerWithoutRounding(hours).
  double h;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, h, ToIntegerWithoutRounding(isolate, hours),
      Handle<JSTemporalDuration>());

  // 7. Let m be ? ToIntegerWithoutRounding(minutes).
  double m;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, m, ToIntegerWithoutRounding(isolate, minutes),
      Handle<JSTemporalDuration>());

  // 8. Let s be ? ToIntegerWithoutRounding(seconds).
  double s;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, s, ToIntegerWithoutRounding(isolate, seconds),
      Handle<JSTemporalDuration>());

  // 9. Let ms be ? ToIntegerWithoutRounding(milliseconds).
  double ms;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, ms, ToIntegerWithoutRounding(isolate, milliseconds),
      Handle<JSTemporalDuration>());

  // 10. Let mis be ? ToIntegerWithoutRounding(microseconds).
  double mis;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, mis, ToIntegerWithoutRounding(isolate, microseconds),
      Handle<JSTemporalDuration>());

  // 11. Let ns be ? ToIntegerWithoutRounding(nanoseconds).
  double ns;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, ns, ToIntegerWithoutRounding(isolate, nanoseconds),
      Handle<JSTemporalDuration>());

  // 12. Return ? CreateTemporalDuration(y, mo, w, d, h, m, s, ms, mis, ns,
  // NewTarget).
  return CreateTemporalDuration(isolate, target, new_target,
                                {y, mo, w, {d, h, m, s, ms, mis, ns}});
}

// #sec-temporal.duration.from
MaybeHandle<JSTemporalDuration> JSTemporalDuration::From(Isolate* isolate,
                                                         Handle<Object> item) {
  //  1. If Type(item) is Object and item has an [[InitializedTemporalDuration]]
  //  internal slot, then
  if (item->IsJSTemporalDuration()) {
    // a. Return ? CreateTemporalDuration(item.[[Years]], item.[[Months]],
    // item.[[Weeks]], item.[[Days]], item.[[Hours]], item.[[Minutes]],
    // item.[[Seconds]], item.[[Milliseconds]], item.[[Microseconds]],
    // item.[[Nanoseconds]]).
    Handle<JSTemporalDuration> duration =
        Handle<JSTemporalDuration>::cast(item);
    return CreateTemporalDuration(
        isolate,
        {duration->years().Number(),
         duration->months().Number(),
         duration->weeks().Number(),
         {duration->days().Number(), duration->hours().Number(),
          duration->minutes().Number(), duration->seconds().Number(),
          duration->milliseconds().Number(), duration->microseconds().Number(),
          duration->nanoseconds().Number()}});
  }
  // 2. Return ? ToTemporalDuration(item).
  return temporal::ToTemporalDuration(isolate, item, "Temporal.Duration.from");
}

// #sec-get-temporal.duration.prototype.sign
MaybeHandle<Smi> JSTemporalDuration::Sign(Isolate* isolate,
                                          Handle<JSTemporalDuration> duration) {
  // 1. Let duration be the this value.
  // 2. Perform ? RequireInternalSlot(duration,
  // [[InitializedTemporalDuration]]).
  // 3. Return ! DurationSign(duration.[[Years]], duration.[[Months]],
  // duration.[[Weeks]], duration.[[Days]], duration.[[Hours]],
  // duration.[[Minutes]], duration.[[Seconds]], duration.[[Milliseconds]],
  // duration.[[Microseconds]], duration.[[Nanoseconds]]).
  return Handle<Smi>(
      Smi::FromInt(DurationSign(
          isolate, {duration->years().Number(),
                    duration->months().Number(),
                    duration->weeks().Number(),
                    {duration->days().Number(), duration->hours().Number(),
                     duration->minutes().Number(), duration->seconds().Number(),
                     duration->milliseconds().Number(),
                     duration->microseconds().Number(),
                     duration->nanoseconds().Number()}})),
      isolate);
}

// #sec-get-temporal.duration.prototype.blank
MaybeHandle<Oddball> JSTemporalDuration::Blank(
    Isolate* isolate, Handle<JSTemporalDuration> duration) {
  // 1. Let duration be the this value.
  // 2. Perform ? RequireInternalSlot(duration,
  // [[InitializedTemporalDuration]]).
  // 3. Let sign be ! DurationSign(duration.[[Years]], duration.[[Months]],
  // duration.[[Weeks]], duration.[[Days]], duration.[[Hours]],
  // duration.[[Minutes]], duration.[[Seconds]], duration.[[Milliseconds]],
  // duration.[[Microseconds]], duration.[[Nanoseconds]]).
  // 4. If sign = 0, return true.
  // 5. Return false.
  int32_t sign = DurationSign(
      isolate,
      {duration->years().Number(),
       duration->months().Number(),
       duration->weeks().Number(),
       {duration->days().Number(), duration->hours().Number(),
        duration->minutes().Number(), duration->seconds().Number(),
        duration->milliseconds().Number(), duration->microseconds().Number(),
        duration->nanoseconds().Number()}});
  return sign == 0 ? isolate->factory()->true_value()
                   : isolate->factory()->false_value();
}

namespace {
// #sec-temporal-createnegatedtemporalduration
MaybeHandle<JSTemporalDuration> CreateNegatedTemporalDuration(
    Isolate* isolate, Handle<JSTemporalDuration> duration) {
  TEMPORAL_ENTER_FUNC();
  // 1. Assert: Type(duration) is Object.
  // 2. Assert: duration has an [[InitializedTemporalDuration]] internal slot.
  // 3. Return ! CreateTemporalDuration(−duration.[[Years]],
  // −duration.[[Months]], −duration.[[Weeks]], −duration.[[Days]],
  // −duration.[[Hours]], −duration.[[Minutes]], −duration.[[Seconds]],
  // −duration.[[Milliseconds]], −duration.[[Microseconds]],
  // −duration.[[Nanoseconds]]).

  return CreateTemporalDuration(
      isolate,
      {-duration->years().Number(),
       -duration->months().Number(),
       -duration->weeks().Number(),
       {-duration->days().Number(), -duration->hours().Number(),
        -duration->minutes().Number(), -duration->seconds().Number(),
        -duration->milliseconds().Number(), -duration->microseconds().Number(),
        -duration->nanoseconds().Number()}});
}

}  // namespace

// #sec-temporal.duration.prototype.negated
MaybeHandle<JSTemporalDuration> JSTemporalDuration::Negated(
    Isolate* isolate, Handle<JSTemporalDuration> duration) {
  // Let duration be the this value.
  // 2. Perform ? RequireInternalSlot(duration,
  // [[InitializedTemporalDuration]]).

  // 3. Return ! CreateNegatedTemporalDuration(duration).
  return CreateNegatedTemporalDuration(isolate, duration);
}

// #sec-temporal.duration.prototype.abs
MaybeHandle<JSTemporalDuration> JSTemporalDuration::Abs(
    Isolate* isolate, Handle<JSTemporalDuration> duration) {
  // 1. Let duration be the this value.
  // 2. Perform ? RequireInternalSlot(duration,
  // [[InitializedTemporalDuration]]).
  // 3. Return ? CreateTemporalDuration(abs(duration.[[Years]]),
  // abs(duration.[[Months]]), abs(duration.[[Weeks]]), abs(duration.[[Days]]),
  // abs(duration.[[Hours]]), abs(duration.[[Minutes]]),
  // abs(duration.[[Seconds]]), abs(duration.[[Milliseconds]]),
  // abs(duration.[[Microseconds]]), abs(duration.[[Nanoseconds]])).
  return CreateTemporalDuration(isolate,
                                {std::abs(duration->years().Number()),
                                 std::abs(duration->months().Number()),
                                 std::abs(duration->weeks().Number()),
                                 {std::abs(duration->days().Number()),
                                  std::abs(duration->hours().Number()),
                                  std::abs(duration->minutes().Number()),
                                  std::abs(duration->seconds().Number()),
                                  std::abs(duration->milliseconds().Number()),
                                  std::abs(duration->microseconds().Number()),
                                  std::abs(duration->nanoseconds().Number())}});
}

// #sec-temporal.calendar
MaybeHandle<JSTemporalCalendar> JSTemporalCalendar::Constructor(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    Handle<Object> identifier_obj) {
  // 1. If NewTarget is undefined, then
  if (new_target->IsUndefined(isolate)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kConstructorNotFunction,
                                 isolate->factory()->NewStringFromStaticChars(
                                     "Temporal.Calendar")),
                    JSTemporalCalendar);
  }
  // 2. Set identifier to ? ToString(identifier).
  Handle<String> identifier;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, identifier,
                             Object::ToString(isolate, identifier_obj),
                             JSTemporalCalendar);
  // 3. If ! IsBuiltinCalendar(id) is false, then
  if (!IsBuiltinCalendar(isolate, identifier)) {
    // a. Throw a RangeError exception.
    THROW_NEW_ERROR(
        isolate, NewRangeError(MessageTemplate::kInvalidCalendar, identifier),
        JSTemporalCalendar);
  }
  return CreateTemporalCalendar(isolate, target, new_target, identifier);
}

namespace {

// #sec-temporal-toisodayofyear
int32_t ToISODayOfYear(Isolate* isolate, const DateRecordCommon& date) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: year is an integer.
  // 2. Assert: month is an integer.
  // 3. Assert: day is an integer.
  // 4. Let date be the date given by year, month, and day.
  // 5. Return date's ordinal date in the year according to ISO-8601.
  // Note: In ISO 8601, Jan: month=1, Dec: month=12,
  // In DateCache API, Jan: month=0, Dec: month=11 so we need to - 1 for month.
  return date.day +
         isolate->date_cache()->DaysFromYearMonth(date.year, date.month - 1) -
         isolate->date_cache()->DaysFromYearMonth(date.year, 0);
}

bool IsPlainDatePlainDateTimeOrPlainYearMonth(
    Handle<Object> temporal_date_like) {
  return temporal_date_like->IsJSTemporalPlainDate() ||
         temporal_date_like->IsJSTemporalPlainDateTime() ||
         temporal_date_like->IsJSTemporalPlainYearMonth();
}

// #sec-temporal-toisodayofweek
int32_t ToISODayOfWeek(Isolate* isolate, int32_t year, int32_t month,
                       int32_t day) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: year is an integer.
  // 2. Assert: month is an integer.
  // 3. Assert: day is an integer.
  // 4. Let date be the date given by year, month, and day.
  // 5. Return date's day of the week according to ISO-8601.
  // Note: In ISO 8601, Jan: month=1, Dec: month=12.
  // In DateCache API, Jan: month=0, Dec: month=11 so we need to - 1 for month.
  // Weekday() expect "the number of days since the epoch" as input and the
  // value of day is 1-based so we need to minus 1 to calculate "the number of
  // days" because the number of days on the epoch (1970/1/1) should be 0,
  // not 1.
  int32_t weekday = isolate->date_cache()->Weekday(
      isolate->date_cache()->DaysFromYearMonth(year, month - 1) + day - 1);
  // Note: In ISO 8601, Sun: weekday=7 Mon: weekday=1
  // In DateCache API, Sun: weekday=0 Mon: weekday=1
  return weekday == 0 ? 7 : weekday;
}

// #sec-temporal-regulateisodate
Maybe<DateRecordCommon> RegulateISODate(Isolate* isolate, ShowOverflow overflow,
                                        const DateRecordCommon& date) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: year, month, and day are integers.
  // 2. Assert: overflow is either "constrain" or "reject".
  switch (overflow) {
    // 3. If overflow is "reject", then
    case ShowOverflow::kReject:
      // a. If ! IsValidISODate(year, month, day) is false, throw a RangeError
      // exception.
      if (!IsValidISODate(isolate, date)) {
        THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                     NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                                     Nothing<DateRecordCommon>());
      }
      // b. Return the Record { [[Year]]: year, [[Month]]: month, [[Day]]: day
      // }.
      return Just(date);
    // 4. If overflow is "constrain", then
    case ShowOverflow::kConstrain:
      DateRecordCommon result(date);
      // a. Set month to ! ConstrainToRange(month, 1, 12).
      result.month = std::max(std::min(result.month, 12), 1);
      // b. Set day to ! ConstrainToRange(day, 1, ! ISODaysInMonth(year,
      // month)).
      result.day =
          std::max(std::min(result.day,
                            ISODaysInMonth(isolate, result.year, result.month)),
                   1);
      // c. Return the Record { [[Year]]: year, [[Month]]: month, [[Day]]: day
      // }.
      return Just(result);
  }
}

// #sec-temporal-regulateisoyearmonth
Maybe<int32_t> RegulateISOYearMonth(Isolate* isolate, ShowOverflow overflow,
                                    int32_t month) {
  // 1. Assert: year and month are integers.
  // 2. Assert: overflow is either "constrain" or "reject".
  switch (overflow) {
    // 3. If overflow is "constrain", then
    case ShowOverflow::kConstrain:
      // a. Return ! ConstrainISOYearMonth(year, month).
      return Just(std::max(std::min(month, 12), 1));
    // 4. If overflow is "reject", then
    case ShowOverflow::kReject:
      // a. If ! IsValidISOMonth(month) is false, throw a RangeError exception.
      if (month < 1 || 12 < month) {
        THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                     NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                                     Nothing<int32_t>());
      }
      // b. Return the new Record { [[Year]]: year, [[Month]]: month }.
      return Just(month);
    default:
      UNREACHABLE();
  }
}

// #sec-temporal-resolveisomonth
Maybe<int32_t> ResolveISOMonth(Isolate* isolate, Handle<JSReceiver> fields) {
  Factory* factory = isolate->factory();
  // 1. Let month be ! Get(fields, "month").
  Handle<Object> month_obj =
      Object::GetPropertyOrElement(isolate, fields, factory->month_string())
          .ToHandleChecked();
  // 2. Let monthCode be ! Get(fields, "monthCode").
  Handle<Object> month_code_obj =
      Object::GetPropertyOrElement(isolate, fields, factory->monthCode_string())
          .ToHandleChecked();
  // 3. If monthCode is undefined, then
  if (month_code_obj->IsUndefined(isolate)) {
    // a. If month is undefined, throw a TypeError exception.
    if (month_obj->IsUndefined(isolate)) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate, NEW_TEMPORAL_INVALID_ARG_TYPE_ERROR(), Nothing<int32_t>());
    }
    // b. Return month.
    // Note: In Temporal spec, "month" in fields is always converted by
    // ToPositiveInteger inside PrepareTemporalFields before calling
    // ResolveISOMonth. Therefore the month_obj is always a positive integer.
    DCHECK(month_obj->IsSmi() || month_obj->IsHeapNumber());
    return Just(FastD2I(month_obj->Number()));
  }
  // 4. Assert: Type(monthCode) is String.
  DCHECK(month_code_obj->IsString());
  Handle<String> month_code;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, month_code,
                                   Object::ToString(isolate, month_code_obj),
                                   Nothing<int32_t>());
  // 5. Let monthLength be the length of monthCode.
  // 6. If monthLength is not 3, throw a RangeError exception.
  if (month_code->length() != 3) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(MessageTemplate::kPropertyValueOutOfRange,
                      factory->monthCode_string()),
        Nothing<int32_t>());
  }
  // 7. Let numberPart be the substring of monthCode from 1.
  // 8. Set numberPart to ! ToIntegerOrInfinity(numberPart).
  // 9. If numberPart < 1 or numberPart > 12, throw a RangeError exception.
  uint16_t m0 = month_code->Get(0);
  uint16_t m1 = month_code->Get(1);
  uint16_t m2 = month_code->Get(2);
  if (!((m0 == 'M') && ((m1 == '0' && '1' <= m2 && m2 <= '9') ||
                        (m1 == '1' && '0' <= m2 && m2 <= '2')))) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(MessageTemplate::kPropertyValueOutOfRange,
                      factory->monthCode_string()),
        Nothing<int32_t>());
  }
  int32_t number_part =
      10 * static_cast<int32_t>(m1 - '0') + static_cast<int32_t>(m2 - '0');
  // 10. If month is not undefined, and month ≠ numberPart, then
  // 11. If ! SameValueNonNumeric(monthCode, ! BuildISOMonthCode(numberPart)) is
  // false, then a. Throw a RangeError exception.
  // Note: In Temporal spec, "month" in fields is always converted by
  // ToPositiveInteger inside PrepareTemporalFields before calling
  // ResolveISOMonth. Therefore the month_obj is always a positive integer.
  if (!month_obj->IsUndefined() &&
      FastD2I(month_obj->Number()) != number_part) {
    // a. Throw a RangeError exception.
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(MessageTemplate::kPropertyValueOutOfRange,
                      factory->month_string()),
        Nothing<int32_t>());
  }

  // 12. Return numberPart.
  return Just(number_part);
}

// #sec-temporal-isodatefromfields
Maybe<DateRecordCommon> ISODateFromFields(Isolate* isolate,
                                          Handle<JSReceiver> fields,
                                          Handle<JSReceiver> options,
                                          const char* method_name) {
  Factory* factory = isolate->factory();

  // 1. Assert: Type(fields) is Object.
  // 2. Let overflow be ? ToTemporalOverflow(options).
  ShowOverflow overflow;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, overflow, ToTemporalOverflow(isolate, options, method_name),
      Nothing<DateRecordCommon>());
  // 3. Set fields to ? PrepareTemporalFields(fields, « "day", "month",
  // "monthCode", "year" », «»).
  Handle<FixedArray> field_names = factory->NewFixedArray(4);
  field_names->set(0, ReadOnlyRoots(isolate).day_string());
  field_names->set(1, ReadOnlyRoots(isolate).month_string());
  field_names->set(2, ReadOnlyRoots(isolate).monthCode_string());
  field_names->set(3, ReadOnlyRoots(isolate).year_string());
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, fields,
      PrepareTemporalFields(isolate, fields, field_names,
                            RequiredFields::kNone),
      Nothing<DateRecordCommon>());

  // 4. Let year be ! Get(fields, "year").
  Handle<Object> year_obj =
      Object::GetPropertyOrElement(isolate, fields, factory->year_string())
          .ToHandleChecked();
  // 5. If year is undefined, throw a TypeError exception.
  if (year_obj->IsUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_VALUE(isolate, NEW_TEMPORAL_INVALID_ARG_TYPE_ERROR(),
                                 Nothing<DateRecordCommon>());
  }
  // Note: "year" in fields is always converted by
  // ToIntegerThrowOnInfinity inside the PrepareTemporalFields above.
  // Therefore the year_obj is always an integer.
  DCHECK(year_obj->IsSmi() || year_obj->IsHeapNumber());

  // 6. Let month be ? ResolveISOMonth(fields).
  int32_t month;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, month,
                                         ResolveISOMonth(isolate, fields),
                                         Nothing<DateRecordCommon>());

  // 7. Let day be ! Get(fields, "day").
  Handle<Object> day_obj =
      Object::GetPropertyOrElement(isolate, fields, factory->day_string())
          .ToHandleChecked();
  // 8. If day is undefined, throw a TypeError exception.
  if (day_obj->IsUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_VALUE(isolate, NEW_TEMPORAL_INVALID_ARG_TYPE_ERROR(),
                                 Nothing<DateRecordCommon>());
  }
  // Note: "day" in fields is always converted by
  // ToIntegerThrowOnInfinity inside the PrepareTemporalFields above.
  // Therefore the day_obj is always an integer.
  DCHECK(day_obj->IsSmi() || day_obj->IsHeapNumber());
  // 9. Return ? RegulateISODate(year, month, day, overflow).
  return RegulateISODate(
      isolate, overflow,
      {FastD2I(year_obj->Number()), month, FastD2I(day_obj->Number())});
}

// #sec-temporal-addisodate
Maybe<DateRecordCommon> AddISODate(Isolate* isolate,
                                   const DateRecordCommon& date,
                                   const DateDurationRecord& duration,
                                   ShowOverflow overflow) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: year, month, day, years, months, weeks, and days are integers.
  // 2. Assert: overflow is either "constrain" or "reject".
  DCHECK(overflow == ShowOverflow::kConstrain ||
         overflow == ShowOverflow::kReject);
  // 3. Let intermediate be ! BalanceISOYearMonth(year + years, month + months).
  DateRecordCommon intermediate = date;
  intermediate.year += static_cast<int32_t>(duration.years);
  intermediate.month += static_cast<int32_t>(duration.months);
  BalanceISOYearMonth(isolate, &intermediate.year, &intermediate.month);
  // 4. Let intermediate be ? RegulateISODate(intermediate.[[Year]],
  // intermediate.[[Month]], day, overflow).
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, intermediate, RegulateISODate(isolate, overflow, intermediate),
      Nothing<DateRecordCommon>());

  // 5. Set days to days + 7 × weeks.
  // 6. Let d be intermediate.[[Day]] + days.
  intermediate.day += duration.days + 7 * duration.weeks;
  // 7. Let intermediate be ! BalanceISODate(intermediate.[[Year]],
  // intermediate.[[Month]], d).
  intermediate = BalanceISODate(isolate, intermediate);
  // 8. Return ? RegulateISODate(intermediate.[[Year]], intermediate.[[Month]],
  // intermediate.[[Day]], overflow).
  return RegulateISODate(isolate, overflow, intermediate);
}

// #sec-temporal-differenceisodate
Maybe<DateDurationRecord> DifferenceISODate(Isolate* isolate,
                                            const DateRecordCommon& date1,
                                            const DateRecordCommon& date2,
                                            Unit largest_unit,
                                            const char* method) {
  TEMPORAL_ENTER_FUNC();

  // 1. Assert: largestUnit is one of "year", "month", "week", or "day".
  DCHECK(largest_unit == Unit::kYear || largest_unit == Unit::kMonth ||
         largest_unit == Unit::kWeek || largest_unit == Unit::kDay);
  // 2. If largestUnit is "year" or "month", then
  switch (largest_unit) {
    case Unit::kYear:
    case Unit::kMonth: {
      // a. Let sign be -(! CompareISODate(y1, m1, d1, y2, m2, d2)).
      int32_t sign = -CompareISODate(isolate, date1, date2);
      // b. If sign is 0, return ! CreateDateDurationRecord(0, 0, 0, 0).
      if (sign == 0) {
        return DateDurationRecord::Create(isolate, 0, 0, 0, 0);
      }

      // c. Let start be the new Record { [[Year]]: y1, [[Month]]: m1, [[Day]]:
      // d1
      // }.
      DateRecordCommon start = date1;
      // d. Let end be the new Record { [[Year]]: y2, [[Month]]: m2, [[Day]]:
      // d2 }.
      DateRecordCommon end = date2;
      // e. Let years be end.[[Year]] − start.[[Year]].
      double years = end.year - start.year;
      // f. Let mid be ! AddISODate(y1, m1, d1, years, 0, 0, 0, "constrain").
      DateRecordCommon mid;
      MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, mid,
          AddISODate(isolate, date1, {years, 0, 0, 0},
                     ShowOverflow::kConstrain),
          Nothing<DateDurationRecord>());

      // g. Let midSign be -(! CompareISODate(mid.[[Year]], mid.[[Month]],
      // mid.[[Day]], y2, m2, d2)).
      int32_t mid_sign = -CompareISODate(isolate, mid, date2);

      // h. If midSign is 0, then
      if (mid_sign == 0) {
        // i. If largestUnit is "year", return ! CreateDateDurationRecord(years,
        // 0, 0, 0).
        if (largest_unit == Unit::kYear) {
          return DateDurationRecord::Create(isolate, years, 0, 0, 0);
        }
        // ii. Return ! CreateDateDurationRecord(0, years × 12, 0, 0).
        return DateDurationRecord::Create(isolate, 0, years * 12, 0, 0);
      }
      // i. Let months be end.[[Month]] − start.[[Month]].
      double months = end.month - start.month;
      // j. If midSign is not equal to sign, then
      if (mid_sign != sign) {
        // i. Set years to years - sign.
        years -= sign;
        // ii. Set months to months + sign × 12.
        months += sign * 12;
      }
      // k. Set mid be ! AddISODate(y1, m1, d1, years, months, 0, 0,
      // "constrain").
      MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, mid,
          AddISODate(isolate, date1, {years, months, 0, 0},
                     ShowOverflow::kConstrain),
          Nothing<DateDurationRecord>());
      // l. Let midSign be -(! CompareISODate(mid.[[Year]], mid.[[Month]],
      // mid.[[Day]], y2, m2, d2)).
      mid_sign = -CompareISODate(isolate, mid, date2);
      // m. If midSign is 0, then
      if (mid_sign == 0) {
        // 1. i. If largestUnit is "year", return !
        // CreateDateDurationRecord(years, months, 0, 0).
        if (largest_unit == Unit::kYear) {
          return DateDurationRecord::Create(isolate, years, months, 0, 0);
        }
        // ii. Return ! CreateDateDurationRecord(0, months + years × 12, 0, 0).
        return DateDurationRecord::Create(isolate, 0, months + years * 12, 0,
                                          0);
      }
      // n. If midSign is not equal to sign, then
      if (mid_sign != sign) {
        // i. Set months to months - sign.
        months -= sign;
        // ii. If months is equal to -sign, then
        if (months == -sign) {
          // 1. Set years to years - sign.
          years -= sign;
          // 2. Set months to 11 × sign.
          months = 11 * sign;
        }
        // iii. Set mid be ! AddISODate(y1, m1, d1, years, months, 0, 0,
        // "constrain").
        MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, mid,
            AddISODate(isolate, date1, {years, months, 0, 0},
                       ShowOverflow::kConstrain),
            Nothing<DateDurationRecord>());
        // iv. Let midSign be -(! CompareISODate(mid.[[Year]], mid.[[Month]],
        // mid.[[Day]], y2, m2, d2)).
        mid_sign = -CompareISODate(isolate, mid, date2);
      }
      // o. Let days be 0.
      double days = 0;
      // p. If mid.[[Month]] = end.[[Month]], then
      if (mid.month == end.month) {
        // i. Assert: mid.[[Year]] = end.[[Year]].
        CHECK_EQ(mid.year, end.year);
        // ii. Set days to end.[[Day]] - mid.[[Day]].
        days = end.day - mid.day;
      } else if (sign < 0) {
        // q. Else if sign < 0, set days to -mid.[[Day]] - (!
        // ISODaysInMonth(end.[[Year]], end.[[Month]]) - end.[[Day]]).
        days =
            -mid.day - (ISODaysInMonth(isolate, end.year, end.month) - end.day);
      } else {
        // r. Else, set days to end.[[Day]] + (! ISODaysInMonth(mid.[[Year]],
        // mid.[[Month]]) - mid.[[Day]]).
        days =
            end.day + (ISODaysInMonth(isolate, mid.year, mid.month) - mid.day);
      }
      // s. If largestUnit is "month", then
      if (largest_unit == Unit::kMonth) {
        // i. Set months to months + years × 12.
        months += years * 12;
        // ii. Set years to 0.
        years = 0;
      }
      // t. Return ! CreateDateDurationRecord(years, months, 0, days).
      return DateDurationRecord::Create(isolate, years, months, 0, days);
    }
      // 3. If largestUnit is "day" or "week", then
    case Unit::kDay:
    case Unit::kWeek: {
      DateRecordCommon smaller, greater;
      // a. If ! CompareISODate(y1, m1, d1, y2, m2, d2) < 0, then
      int32_t sign;
      if (CompareISODate(isolate, date1, date2) < 0) {
        // i. Let smaller be the Record { [[Year]]: y1, [[Month]]: m1, [[Day]]:
        // d1
        // }.
        smaller = date1;
        // ii. Let greater be the Record { [[Year]]: y2, [[Month]]: m2, [[Day]]:
        // d2
        // }.
        greater = date2;
        // iii. Let sign be 1.
        sign = 1;
      } else {
        // b. Else,
        // i. Let smaller be the new Record { [[Year]]: y2, [[Month]]: m2,
        // [[Day]]: d2 }.
        smaller = date2;
        // ii. Let greater be the new Record { [[Year]]: y1, [[Month]]: m1,
        // [[Day]]: d1 }.
        greater = date1;
        // iii. Let sign be −1.
        sign = -1;
      }
      // c. Let days be ! ToISODayOfYear(greater.[[Year]], greater.[[Month]],
      // greater.[[Day]]) − ! ToISODayOfYear(smaller.[[Year]],
      // smaller.[[Month]], smaller.[[Day]]).
      int32_t days =
          ToISODayOfYear(isolate, greater) - ToISODayOfYear(isolate, smaller);
      // d. Let year be smaller.[[Year]].
      // e. Repeat, while year < greater.[[Year]],
      for (int32_t year = smaller.year; year < greater.year; year++) {
        // i. Set days to days + ! ISODaysInYear(year).
        // ii. Set year to year + 1.
        days += ISODaysInYear(isolate, year);
      }
      // f. Let weeks be 0.
      int32_t weeks = 0;
      // g. If largestUnit is "week", then
      if (largest_unit == Unit::kWeek) {
        // i. Set weeks to floor(days / 7).
        weeks = days / 7;
        // ii. Set days to days mod 7.
        days = days % 7;
      }
      // h. Return ! CreateDateDurationRecord(0, 0, weeks × sign, days × sign).
      return DateDurationRecord::Create(isolate, 0, 0, weeks * sign,
                                        days * sign);
    }
    default:
      UNREACHABLE();
  }
}

// #sec-temporal-isoyearmonthfromfields
Maybe<DateRecordCommon> ISOYearMonthFromFields(Isolate* isolate,
                                               Handle<JSReceiver> fields,
                                               Handle<JSReceiver> options,
                                               const char* method_name) {
  Factory* factory = isolate->factory();
  // 1. Assert: Type(fields) is Object.
  // 2. Let overflow be ? ToTemporalOverflow(options).
  ShowOverflow overflow;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, overflow, ToTemporalOverflow(isolate, options, method_name),
      Nothing<DateRecordCommon>());
  // 3. Set fields to ? PrepareTemporalFields(fields, « "month", "monthCode",
  // "year" », «»).
  Handle<FixedArray> field_names = factory->NewFixedArray(3);
  field_names->set(0, ReadOnlyRoots(isolate).month_string());
  field_names->set(1, ReadOnlyRoots(isolate).monthCode_string());
  field_names->set(2, ReadOnlyRoots(isolate).year_string());
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, fields,
      PrepareTemporalFields(isolate, fields, field_names,
                            RequiredFields::kNone),
      Nothing<DateRecordCommon>());

  // 4. Let year be ! Get(fields, "year").
  Handle<Object> year_obj =
      Object::GetPropertyOrElement(isolate, fields, factory->year_string())
          .ToHandleChecked();
  // 5. If year is undefined, throw a TypeError exception.
  if (year_obj->IsUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_VALUE(isolate, NEW_TEMPORAL_INVALID_ARG_TYPE_ERROR(),
                                 Nothing<DateRecordCommon>());
  }
  DateRecordCommon result;
  result.year = FastD2I(floor(year_obj->Number()));
  // 6. Let month be ? ResolveISOMonth(fields).
  int32_t month;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, month,
                                         ResolveISOMonth(isolate, fields),
                                         Nothing<DateRecordCommon>());
  // 7. Let result be ? RegulateISOYearMonth(year, month, overflow).
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, result.month, RegulateISOYearMonth(isolate, overflow, month),
      Nothing<DateRecordCommon>());
  // 8. Return the new Record { [[Year]]: result.[[Year]], [[Month]]:
  // result.[[Month]], [[ReferenceISODay]]: 1 }.
  result.day = 1;
  return Just(result);
}

}  // namespace

// #sec-temporal.calendar.prototype.dateadd
MaybeHandle<JSTemporalPlainDate> JSTemporalCalendar::DateAdd(
    Isolate* isolate, Handle<JSTemporalCalendar> calendar,
    Handle<Object> date_obj, Handle<Object> duration_obj,
    Handle<Object> options_obj) {
  const char* method_name = "Temporal.Calendar.prototype.dateAdd";
  // 1. Let calendar be the this value.
  // 2. Perform ? RequireInternalSlot(calendar,
  // [[InitializedTemporalCalendar]]).
  // 3. Assert: calendar.[[Identifier]] is "iso8601".
  // 4. Set date to ? ToTemporalDate(date).
  Handle<JSTemporalPlainDate> date;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, date,
                             ToTemporalDate(isolate, date_obj, method_name),
                             JSTemporalPlainDate);

  // 5. Set duration to ? ToTemporalDuration(duration).
  Handle<JSTemporalDuration> duration;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, duration,
      temporal::ToTemporalDuration(isolate, duration_obj, method_name),
      JSTemporalPlainDate);

  // 6. Set options to ? GetOptionsObject(options).
  Handle<JSReceiver> options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, GetOptionsObject(isolate, options_obj, method_name),
      JSTemporalPlainDate);

  // 7. Let overflow be ? ToTemporalOverflow(options).
  ShowOverflow overflow;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, overflow, ToTemporalOverflow(isolate, options, method_name),
      Handle<JSTemporalPlainDate>());

  // 8. Let balanceResult be ! BalanceDuration(duration.[[Days]],
  // duration.[[Hours]], duration.[[Minutes]], duration.[[Seconds]],
  // duration.[[Milliseconds]], duration.[[Microseconds]],
  // duration.[[Nanoseconds]], "day").
  TimeDurationRecord balance_result;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, balance_result,
      BalanceDuration(
          isolate, Unit::kDay,
          {duration->days().Number(), duration->hours().Number(),
           duration->minutes().Number(), duration->seconds().Number(),
           duration->milliseconds().Number(), duration->microseconds().Number(),
           duration->nanoseconds().Number()},
          method_name),
      Handle<JSTemporalPlainDate>());
  DateRecordCommon result;
  // If calendar.[[Identifier]] is "iso8601", then
  if (calendar->calendar_index() == 0) {
    // 9. Let result be ? AddISODate(date.[[ISOYear]], date.[[ISOMonth]],
    // date.[[ISODay]], duration.[[Years]], duration.[[Months]],
    // duration.[[Weeks]], balanceResult.[[Days]], overflow).
    MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, result,
        AddISODate(isolate,
                   {date->iso_year(), date->iso_month(), date->iso_day()},
                   {duration->years().Number(), duration->months().Number(),
                    duration->weeks().Number(), balance_result.days},
                   overflow),
        Handle<JSTemporalPlainDate>());
  } else {
#ifdef V8_INTL_SUPPORT
    // TODO(ftang) add code for other calendar.
    UNIMPLEMENTED();
#else   // V8_INTL_SUPPORT
    UNREACHABLE();
#endif  // V8_INTL_SUPPORT
  }
  // 10. Return ? CreateTemporalDate(result.[[Year]], result.[[Month]],
  // result.[[Day]], calendar).
  return CreateTemporalDate(isolate, result, calendar);
}

// #sec-temporal.calendar.prototype.daysinyear
MaybeHandle<Smi> JSTemporalCalendar::DaysInYear(
    Isolate* isolate, Handle<JSTemporalCalendar> calendar,
    Handle<Object> temporal_date_like) {
  // 1. Let calendar be the this value.
  // 2. Perform ? RequireInternalSlot(calendar,
  // [[InitializedTemporalCalendar]]).
  // 3. Assert: calendar.[[Identifier]] is "iso8601".
  // 4. If Type(temporalDateLike) is not Object or temporalDateLike does not
  // have an [[InitializedTemporalDate]], [[InitializedTemporalDateTime]] or
  // [[InitializedTemporalYearMonth]] internal slot, then
  if (!IsPlainDatePlainDateTimeOrPlainYearMonth(temporal_date_like)) {
    // a. Set temporalDateLike to ? ToTemporalDate(temporalDateLike).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, temporal_date_like,
        ToTemporalDate(isolate, temporal_date_like,
                       "Temporal.Calendar.prototype.daysInYear"),
        Smi);
  }

  // a. Let daysInYear be ! ISODaysInYear(temporalDateLike.[[ISOYear]]).
  int32_t year;
  if (temporal_date_like->IsJSTemporalPlainDate()) {
    year = Handle<JSTemporalPlainDate>::cast(temporal_date_like)->iso_year();
  } else if (temporal_date_like->IsJSTemporalPlainDateTime()) {
    year =
        Handle<JSTemporalPlainDateTime>::cast(temporal_date_like)->iso_year();
  } else {
    DCHECK(temporal_date_like->IsJSTemporalPlainYearMonth());
    year =
        Handle<JSTemporalPlainYearMonth>::cast(temporal_date_like)->iso_year();
  }
  int32_t days_in_year = ISODaysInYear(isolate, year);
  // 6. Return 𝔽(daysInYear).
  return handle(Smi::FromInt(days_in_year), isolate);
}

// #sec-temporal.calendar.prototype.daysinmonth
MaybeHandle<Smi> JSTemporalCalendar::DaysInMonth(
    Isolate* isolate, Handle<JSTemporalCalendar> calendar,
    Handle<Object> temporal_date_like) {
  // 1 Let calendar be the this value.
  // 2. Perform ? RequireInternalSlot(calendar,
  // [[InitializedTemporalCalendar]]).
  // 3. Assert: calendar.[[Identifier]] is "iso8601".
  // 4. If Type(temporalDateLike) is not Object or temporalDateLike does not
  // have an [[InitializedTemporalDate]], [[InitializedTemporalDateTime]] or
  // [[InitializedTemporalYearMonth]] internal slot, then
  if (!IsPlainDatePlainDateTimeOrPlainYearMonth(temporal_date_like)) {
    // a. Set temporalDateLike to ? ToTemporalDate(temporalDateLike).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, temporal_date_like,
        ToTemporalDate(isolate, temporal_date_like,
                       "Temporal.Calendar.prototype.daysInMonth"),
        Smi);
  }

  // 5. Return 𝔽(! ISODaysInMonth(temporalDateLike.[[ISOYear]],
  // temporalDateLike.[[ISOMonth]])).
  int32_t year;
  int32_t month;
  if (temporal_date_like->IsJSTemporalPlainDate()) {
    year = Handle<JSTemporalPlainDate>::cast(temporal_date_like)->iso_year();
    month = Handle<JSTemporalPlainDate>::cast(temporal_date_like)->iso_month();
  } else if (temporal_date_like->IsJSTemporalPlainDateTime()) {
    year =
        Handle<JSTemporalPlainDateTime>::cast(temporal_date_like)->iso_year();
    month =
        Handle<JSTemporalPlainDateTime>::cast(temporal_date_like)->iso_month();
  } else {
    DCHECK(temporal_date_like->IsJSTemporalPlainYearMonth());
    year =
        Handle<JSTemporalPlainYearMonth>::cast(temporal_date_like)->iso_year();
    month =
        Handle<JSTemporalPlainYearMonth>::cast(temporal_date_like)->iso_month();
  }
  return handle(Smi::FromInt(ISODaysInMonth(isolate, year, month)), isolate);
}

// #sec-temporal.calendar.prototype.year
MaybeHandle<Smi> JSTemporalCalendar::Year(Isolate* isolate,
                                          Handle<JSTemporalCalendar> calendar,
                                          Handle<Object> temporal_date_like) {
  // 1. Let calendar be the this value.
  // 2. Perform ? RequireInternalSlot(calendar,
  // [[InitializedTemporalCalendar]]).
  // 3. Assert: calendar.[[Identifier]] is "iso8601".
  // 4. If Type(temporalDateLike) is not Object or temporalDateLike does not
  // have an [[InitializedTemporalDate]], [[InitializedTemporalDateTime]],
  // or [[InitializedTemporalYearMonth]]
  // internal slot, then
  if (!IsPlainDatePlainDateTimeOrPlainYearMonth(temporal_date_like)) {
    // a. Set temporalDateLike to ? ToTemporalDate(temporalDateLike).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, temporal_date_like,
        ToTemporalDate(isolate, temporal_date_like,
                       "Temporal.Calendar.prototype.year"),
        Smi);
  }

  // a. Let year be ! ISOYear(temporalDateLike).
  int32_t year;
  if (temporal_date_like->IsJSTemporalPlainDate()) {
    year = Handle<JSTemporalPlainDate>::cast(temporal_date_like)->iso_year();
  } else if (temporal_date_like->IsJSTemporalPlainDateTime()) {
    year =
        Handle<JSTemporalPlainDateTime>::cast(temporal_date_like)->iso_year();
  } else {
    DCHECK(temporal_date_like->IsJSTemporalPlainYearMonth());
    year =
        Handle<JSTemporalPlainYearMonth>::cast(temporal_date_like)->iso_year();
  }

  // 6. Return 𝔽(year).
  return handle(Smi::FromInt(year), isolate);
}

// #sec-temporal.calendar.prototype.dayofyear
MaybeHandle<Smi> JSTemporalCalendar::DayOfYear(
    Isolate* isolate, Handle<JSTemporalCalendar> calendar,
    Handle<Object> temporal_date_like) {
  // 1. Let calendar be the this value.
  // 2. Perform ? RequireInternalSlot(calendar,
  // [[InitializedTemporalCalendar]]).
  // 3. Assert: calendar.[[Identifier]] is "iso8601".
  // 4. Let temporalDate be ? ToTemporalDate(temporalDateLike).
  Handle<JSTemporalPlainDate> temporal_date;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, temporal_date,
      ToTemporalDate(isolate, temporal_date_like,
                     "Temporal.Calendar.prototype.dayOfYear"),
      Smi);
  // a. Let value be ! ToISODayOfYear(temporalDate.[[ISOYear]],
  // temporalDate.[[ISOMonth]], temporalDate.[[ISODay]]).
  int32_t value = ToISODayOfYear(
      isolate, {temporal_date->iso_year(), temporal_date->iso_month(),
                temporal_date->iso_day()});
  return handle(Smi::FromInt(value), isolate);
}

// #sec-temporal.calendar.prototype.dayofweek
MaybeHandle<Smi> JSTemporalCalendar::DayOfWeek(
    Isolate* isolate, Handle<JSTemporalCalendar> calendar,
    Handle<Object> temporal_date_like) {
  // 1. Let calendar be the this value.
  // 2. Perform ? RequireInternalSlot(calendar,
  // [[InitializedTemporalCalendar]]).
  // 3. Assert: calendar.[[Identifier]] is "iso8601".
  // 4. Let temporalDate be ? ToTemporalDate(temporalDateLike).
  Handle<JSTemporalPlainDate> temporal_date;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, temporal_date,
      ToTemporalDate(isolate, temporal_date_like,
                     "Temporal.Calendar.prototype.dayOfWeek"),
      Smi);
  // a. Let value be ! ToISODayOfWeek(temporalDate.[[ISOYear]],
  // temporalDate.[[ISOMonth]], temporalDate.[[ISODay]]).
  int32_t value =
      ToISODayOfWeek(isolate, temporal_date->iso_year(),
                     temporal_date->iso_month(), temporal_date->iso_day());
  return handle(Smi::FromInt(value), isolate);
}

// #sec-temporal.calendar.prototype.monthsinyear
MaybeHandle<Smi> JSTemporalCalendar::MonthsInYear(
    Isolate* isolate, Handle<JSTemporalCalendar> calendar,
    Handle<Object> temporal_date_like) {
  // 1. Let calendar be the this value.
  // 2. Perform ? RequireInternalSlot(calendar,
  // [[InitializedTemporalCalendar]]).
  // 3. Assert: calendar.[[Identifier]] is "iso8601".
  // 4. If Type(temporalDateLike) is not Object or temporalDateLike does not
  // have an [[InitializedTemporalDate]], [[InitializedTemporalDateTime]], or
  // [[InitializedTemporalYearMonth]] internal slot, then
  if (!IsPlainDatePlainDateTimeOrPlainYearMonth(temporal_date_like)) {
    // a. Set temporalDateLike to ? ToTemporalDate(temporalDateLike).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, temporal_date_like,
        ToTemporalDate(isolate, temporal_date_like,
                       "Temporal.Calendar.prototype.monthsInYear"),
        Smi);
  }

  // a. a. Let monthsInYear be 12.
  int32_t months_in_year = 12;
  // 6. Return 𝔽(monthsInYear).
  return handle(Smi::FromInt(months_in_year), isolate);
}

// #sec-temporal.calendar.prototype.inleapyear
MaybeHandle<Oddball> JSTemporalCalendar::InLeapYear(
    Isolate* isolate, Handle<JSTemporalCalendar> calendar,
    Handle<Object> temporal_date_like) {
  // 1. Let calendar be the this value.
  // 2. Perform ? RequireInternalSlot(calendar,
  // [[InitializedTemporalCalendar]]).
  // 3. Assert: calendar.[[Identifier]] is "iso8601".
  // 4. If Type(temporalDateLike) is not Object or temporalDateLike does not
  // have an [[InitializedTemporalDate]], [[InitializedTemporalDateTime]], or
  // [[InitializedTemporalYearMonth]] internal slot, then
  if (!IsPlainDatePlainDateTimeOrPlainYearMonth(temporal_date_like)) {
    // a. Set temporalDateLike to ? ToTemporalDate(temporalDateLike).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, temporal_date_like,
        ToTemporalDate(isolate, temporal_date_like,
                       "Temporal.Calendar.prototype.inLeapYear"),
        Oddball);
  }

  // a. Let inLeapYear be ! IsISOLeapYear(temporalDateLike.[[ISOYear]]).
  int32_t year;
  if (temporal_date_like->IsJSTemporalPlainDate()) {
    year = Handle<JSTemporalPlainDate>::cast(temporal_date_like)->iso_year();
  } else if (temporal_date_like->IsJSTemporalPlainDateTime()) {
    year =
        Handle<JSTemporalPlainDateTime>::cast(temporal_date_like)->iso_year();
  } else {
    DCHECK(temporal_date_like->IsJSTemporalPlainYearMonth());
    year =
        Handle<JSTemporalPlainYearMonth>::cast(temporal_date_like)->iso_year();
  }
  return isolate->factory()->ToBoolean(IsISOLeapYear(isolate, year));
}

// #sec-temporal.calendar.prototype.daysinweek
MaybeHandle<Smi> JSTemporalCalendar::DaysInWeek(
    Isolate* isolate, Handle<JSTemporalCalendar> calendar,
    Handle<Object> temporal_date_like) {
  // 1. Let calendar be the this value.
  // 2. Perform ? RequireInternalSlot(calendar,
  // [[InitializedTemporalCalendar]]).
  // 3. Assert: calendar.[[Identifier]] is "iso8601".
  // 4. Perform ? ToTemporalDate(temporalDateLike).
  Handle<JSTemporalPlainDate> date;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, date,
      ToTemporalDate(isolate, temporal_date_like,
                     "Temporal.Calendar.prototype.daysInWeek"),
      Smi);
  // 5. Return 7𝔽.
  return handle(Smi::FromInt(7), isolate);
}

// #sec-temporal.calendar.prototype.datefromfields
MaybeHandle<JSTemporalPlainDate> JSTemporalCalendar::DateFromFields(
    Isolate* isolate, Handle<JSTemporalCalendar> calendar,
    Handle<Object> fields_obj, Handle<Object> options_obj) {
  // 1. Let calendar be the this value.
  // 2. Perform ? RequireInternalSlot(calendar,
  // [[InitializedTemporalCalendar]]).
  // 3. Assert: calendar.[[Identifier]] is "iso8601".
  // 4. If Type(fields) is not Object, throw a TypeError exception.
  const char* method_name = "Temporal.Calendar.prototype.dateFromFields";
  if (!fields_obj->IsJSReceiver()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kCalledOnNonObject,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     method_name)),
                    JSTemporalPlainDate);
  }
  Handle<JSReceiver> fields = Handle<JSReceiver>::cast(fields_obj);

  // 5. Set options to ? GetOptionsObject(options).
  Handle<JSReceiver> options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, GetOptionsObject(isolate, options_obj, method_name),
      JSTemporalPlainDate);
  if (calendar->calendar_index() == 0) {
    // 6. Let result be ? ISODateFromFields(fields, options).
    DateRecordCommon result;
    MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, result,
        ISODateFromFields(isolate, fields, options, method_name),
        Handle<JSTemporalPlainDate>());
    // 7. Return ? CreateTemporalDate(result.[[Year]], result.[[Month]],
    // result.[[Day]], calendar).
    return CreateTemporalDate(isolate, result, calendar);
  }
  // TODO(ftang) add intl implementation inside #ifdef V8_INTL_SUPPORT
  UNREACHABLE();
}

// #sec-temporal.calendar.prototype.mergefields
MaybeHandle<JSReceiver> JSTemporalCalendar::MergeFields(
    Isolate* isolate, Handle<JSTemporalCalendar> calendar,
    Handle<Object> fields_obj, Handle<Object> additional_fields_obj) {
  // 1. Let calendar be the this value.
  // 2. Perform ? RequireInternalSlot(calendar,
  // [[InitializedTemporalCalendar]]).
  // 3. Assert: calendar.[[Identifier]] is "iso8601".
  // 4. Set fields to ? ToObject(fields).
  Handle<JSReceiver> fields;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, fields,
                             Object::ToObject(isolate, fields_obj), JSReceiver);

  // 5. Set additionalFields to ? ToObject(additionalFields).
  Handle<JSReceiver> additional_fields;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, additional_fields,
                             Object::ToObject(isolate, additional_fields_obj),
                             JSReceiver);
  // 5. If calendar.[[Identifier]] is "iso8601", then
  if (calendar->calendar_index() == 0) {
    // a. Return ? DefaultMergeFields(fields, additionalFields).
    return DefaultMergeFields(isolate, fields, additional_fields);
  }
#ifdef V8_INTL_SUPPORT
  // TODO(ftang) add Intl code.
#endif  // V8_INTL_SUPPORT
  UNREACHABLE();
}

// #sec-temporal.calendar.prototype.dateuntil
MaybeHandle<JSTemporalDuration> JSTemporalCalendar::DateUntil(
    Isolate* isolate, Handle<JSTemporalCalendar> calendar,
    Handle<Object> one_obj, Handle<Object> two_obj,
    Handle<Object> options_obj) {
  const char* method_name = "Temporal.Calendar.prototype.dateUntil";
  // 1. Let calendar be the this value.
  // 2. Perform ? RequireInternalSlot(calendar,
  // [[InitializedTemporalCalendar]]).
  // 3. Assert: calendar.[[Identifier]] is "iso8601".
  // 4. Set one to ? ToTemporalDate(one).
  Handle<JSTemporalPlainDate> one;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, one,
                             ToTemporalDate(isolate, one_obj, method_name),
                             JSTemporalDuration);
  // 5. Set two to ? ToTemporalDate(two).
  Handle<JSTemporalPlainDate> two;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, two,
                             ToTemporalDate(isolate, two_obj, method_name),
                             JSTemporalDuration);
  // 6. Set options to ? GetOptionsObject(options).
  Handle<JSReceiver> options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, GetOptionsObject(isolate, options_obj, method_name),
      JSTemporalDuration);
  // 7. Let largestUnit be ? ToLargestTemporalUnit(options, « "hour", "minute",
  // "second", "millisecond", "microsecond", "nanosecond" », "auto", "day").
  Unit largest_unit;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, largest_unit,
      ToLargestTemporalUnit(
          isolate, options,
          std::set<Unit>({Unit::kHour, Unit::kMinute, Unit::kSecond,
                          Unit::kMillisecond, Unit::kMicrosecond,
                          Unit::kNanosecond}),
          Unit::kAuto, Unit::kDay, method_name),
      Handle<JSTemporalDuration>());

  // 8. Let result be ! DifferenceISODate(one.[[ISOYear]], one.[[ISOMonth]],
  // one.[[ISODay]], two.[[ISOYear]], two.[[ISOMonth]], two.[[ISODay]],
  // largestUnit).
  DateDurationRecord result;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, result,
      DifferenceISODate(isolate,
                        {one->iso_year(), one->iso_month(), one->iso_day()},
                        {two->iso_year(), two->iso_month(), two->iso_day()},
                        largest_unit, method_name),
      Handle<JSTemporalDuration>());

  // 9. Return ! CreateTemporalDuration(result.[[Years]], result.[[Months]],
  // result.[[Weeks]], result.[[Days]], 0, 0, 0, 0, 0, 0).
  return CreateTemporalDuration(isolate, {result.years,
                                          result.months,
                                          result.weeks,
                                          {result.days, 0, 0, 0, 0, 0, 0}});
}

// #sec-temporal.calendar.prototype.day
MaybeHandle<Smi> JSTemporalCalendar::Day(Isolate* isolate,
                                         Handle<JSTemporalCalendar> calendar,
                                         Handle<Object> temporal_date_like) {
  // 1. Let calendar be the this value.
  // 2. Perform ? RequireInternalSlot(calendar,
  // [[InitializedTemporalCalendar]]).
  // 3. Assert: calendar.[[Identifier]] is "iso8601".
  // 4. If Type(temporalDateLike) is not Object or temporalDateLike does not
  // have an [[InitializedTemporalDate]] or [[InitializedTemporalMonthDay]]
  // internal slot, then
  if (!(temporal_date_like->IsJSTemporalPlainDate() ||
        temporal_date_like->IsJSTemporalPlainDateTime() ||
        temporal_date_like->IsJSTemporalPlainMonthDay())) {
    // a. Set temporalDateLike to ? ToTemporalDate(temporalDateLike).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, temporal_date_like,
        ToTemporalDate(isolate, temporal_date_like,
                       "Temporal.Calendar.prototype.day"),
        Smi);
  }

  // 5. Let day be ! ISODay(temporalDateLike).
  int32_t day;
  if (temporal_date_like->IsJSTemporalPlainDate()) {
    day = Handle<JSTemporalPlainDate>::cast(temporal_date_like)->iso_day();
  } else if (temporal_date_like->IsJSTemporalPlainDateTime()) {
    day = Handle<JSTemporalPlainDateTime>::cast(temporal_date_like)->iso_day();
  } else {
    DCHECK(temporal_date_like->IsJSTemporalPlainMonthDay());
    day = Handle<JSTemporalPlainMonthDay>::cast(temporal_date_like)->iso_day();
  }

  // 6. Return 𝔽(day).
  return handle(Smi::FromInt(day), isolate);
}

// #sec-temporal.calendar.prototype.monthcode
MaybeHandle<String> JSTemporalCalendar::MonthCode(
    Isolate* isolate, Handle<JSTemporalCalendar> calendar,
    Handle<Object> temporal_date_like) {
  // 1. Let calendar be the this value.
  // 2. Perform ? RequireInternalSlot(calendar,
  // [[InitializedTemporalCalendar]]).
  // 3. Assert: calendar.[[Identifier]] is "iso8601".
  // 4. If Type(temporalDateLike) is not Object or temporalDateLike does not
  // have an [[InitializedTemporalDate]], [[InitializedTemporalDateTime]],
  // [[InitializedTemporalMonthDay]], or
  // [[InitializedTemporalYearMonth]] internal slot, then
  if (!(IsPlainDatePlainDateTimeOrPlainYearMonth(temporal_date_like) ||
        temporal_date_like->IsJSTemporalPlainMonthDay())) {
    // a. Set temporalDateLike to ? ToTemporalDate(temporalDateLike).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, temporal_date_like,
        ToTemporalDate(isolate, temporal_date_like,
                       "Temporal.Calendar.prototype.monthCode"),
        String);
  }

  // 5. Return ! ISOMonthCode(temporalDateLike).
  int32_t month;
  if (temporal_date_like->IsJSTemporalPlainDate()) {
    month = Handle<JSTemporalPlainDate>::cast(temporal_date_like)->iso_month();
  } else if (temporal_date_like->IsJSTemporalPlainDateTime()) {
    month =
        Handle<JSTemporalPlainDateTime>::cast(temporal_date_like)->iso_month();
  } else if (temporal_date_like->IsJSTemporalPlainMonthDay()) {
    month =
        Handle<JSTemporalPlainMonthDay>::cast(temporal_date_like)->iso_month();
  } else {
    DCHECK(temporal_date_like->IsJSTemporalPlainYearMonth());
    month =
        Handle<JSTemporalPlainYearMonth>::cast(temporal_date_like)->iso_month();
  }
  IncrementalStringBuilder builder(isolate);
  builder.AppendCString((month < 10) ? "M0" : "M");
  builder.AppendInt(month);

  return builder.Finish();
}

// #sec-temporal.calendar.prototype.month
MaybeHandle<Smi> JSTemporalCalendar::Month(Isolate* isolate,
                                           Handle<JSTemporalCalendar> calendar,
                                           Handle<Object> temporal_date_like) {
  // 4. If Type(temporalDateLike) is Object and temporalDateLike has an
  // [[InitializedTemporalMonthDay]] internal slot, then
  if (temporal_date_like->IsJSTemporalPlainMonthDay()) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_TYPE_ERROR(), Smi);
  }
  // 5. If Type(temporalDateLike) is not Object or temporalDateLike does not
  // have an [[InitializedTemporalDate]], [[InitializedTemporalDateTime]],
  // or [[InitializedTemporalYearMonth]]
  // internal slot, then
  if (!IsPlainDatePlainDateTimeOrPlainYearMonth(temporal_date_like)) {
    // a. Set temporalDateLike to ? ToTemporalDate(temporalDateLike).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, temporal_date_like,
        ToTemporalDate(isolate, temporal_date_like,
                       "Temporal.Calendar.prototype.month"),
        Smi);
  }

  // 6. Return ! ISOMonth(temporalDateLike).
  int32_t month;
  if (temporal_date_like->IsJSTemporalPlainDate()) {
    month = Handle<JSTemporalPlainDate>::cast(temporal_date_like)->iso_month();
  } else if (temporal_date_like->IsJSTemporalPlainDateTime()) {
    month =
        Handle<JSTemporalPlainDateTime>::cast(temporal_date_like)->iso_month();
  } else {
    DCHECK(temporal_date_like->IsJSTemporalPlainYearMonth());
    month =
        Handle<JSTemporalPlainYearMonth>::cast(temporal_date_like)->iso_month();
  }

  // 7. Return 𝔽(month).
  return handle(Smi::FromInt(month), isolate);
}

// #sec-temporal.calendar.prototype.monthdayfromfields
MaybeHandle<JSTemporalPlainMonthDay> JSTemporalCalendar::MonthDayFromFields(
    Isolate* isolate, Handle<JSTemporalCalendar> calendar,
    Handle<Object> fields_obj, Handle<Object> options_obj) {
  // 1. Let calendar be the this value.
  // 2. Perform ? RequireInternalSlot(calendar,
  // [[InitializedTemporalCalendar]]).
  // 3. Assert: calendar.[[Identifier]] is "iso8601".
  const char* method_name = "Temporal.Calendar.prototype.monthDayFromFields";
  // 4. If Type(fields) is not Object, throw a TypeError exception.
  if (!fields_obj->IsJSReceiver()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kCalledOnNonObject,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     method_name)),
                    JSTemporalPlainMonthDay);
  }
  Handle<JSReceiver> fields = Handle<JSReceiver>::cast(fields_obj);
  // 5. Set options to ? GetOptionsObject(options).
  Handle<JSReceiver> options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, GetOptionsObject(isolate, options_obj, method_name),
      JSTemporalPlainMonthDay);
  // 6. Let result be ? ISOMonthDayFromFields(fields, options).
  if (calendar->calendar_index() == 0) {
    Maybe<DateRecordCommon> maybe_result =
        ISOMonthDayFromFields(isolate, fields, options, method_name);
    MAYBE_RETURN(maybe_result, Handle<JSTemporalPlainMonthDay>());
    DateRecordCommon result = maybe_result.FromJust();
    // 7. Return ? CreateTemporalMonthDay(result.[[Month]], result.[[Day]],
    // calendar, result.[[ReferenceISOYear]]).
    return CreateTemporalMonthDay(isolate, result.month, result.day, calendar,
                                  result.year);
  }
  // TODO(ftang) add intl code inside #ifdef V8_INTL_SUPPORT
  UNREACHABLE();
}

// #sec-temporal.calendar.prototype.yearmonthfromfields
MaybeHandle<JSTemporalPlainYearMonth> JSTemporalCalendar::YearMonthFromFields(
    Isolate* isolate, Handle<JSTemporalCalendar> calendar,
    Handle<Object> fields_obj, Handle<Object> options_obj) {
  // 1. Let calendar be the this value.
  // 2. Perform ? RequireInternalSlot(calendar,
  // [[InitializedTemporalCalendar]]).
  // 3. Assert: calendar.[[Identifier]] is "iso8601".
  const char* method_name = "Temporal.Calendar.prototype.yearMonthFromFields";
  // 4. If Type(fields) is not Object, throw a TypeError exception.
  if (!fields_obj->IsJSReceiver()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kCalledOnNonObject,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     method_name)),
                    JSTemporalPlainYearMonth);
  }
  Handle<JSReceiver> fields = Handle<JSReceiver>::cast(fields_obj);
  // 5. Set options to ? GetOptionsObject(options).
  Handle<JSReceiver> options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, GetOptionsObject(isolate, options_obj, method_name),
      JSTemporalPlainYearMonth);
  // 6. Let result be ? ISOYearMonthFromFields(fields, options).
  if (calendar->calendar_index() == 0) {
    DateRecordCommon result;
    MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, result,
        ISOYearMonthFromFields(isolate, fields, options, method_name),
        Handle<JSTemporalPlainYearMonth>());
    // 7. Return ? CreateTemporalYearMonth(result.[[Year]], result.[[Month]],
    // calendar, result.[[ReferenceISODay]]).
    return CreateTemporalYearMonth(isolate, result.year, result.month, calendar,
                                   result.day);
  }
  // TODO(ftang) add intl code inside #ifdef V8_INTL_SUPPORT
  UNREACHABLE();
}

// #sec-temporal.calendar.prototype.tostring
MaybeHandle<String> JSTemporalCalendar::ToString(
    Isolate* isolate, Handle<JSTemporalCalendar> calendar,
    const char* method_name) {
  return CalendarIdentifier(isolate, calendar->calendar_index());
}

// #sec-temporal.now.timezone
MaybeHandle<JSTemporalTimeZone> JSTemporalTimeZone::Now(Isolate* isolate) {
  return SystemTimeZone(isolate);
}

// #sec-temporal.timezone
MaybeHandle<JSTemporalTimeZone> JSTemporalTimeZone::Constructor(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    Handle<Object> identifier_obj) {
  // 1. If NewTarget is undefined, then
  if (new_target->IsUndefined(isolate)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kConstructorNotFunction,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     "Temporal.TimeZone")),
                    JSTemporalTimeZone);
  }
  // 2. Set identifier to ? ToString(identifier).
  Handle<String> identifier;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, identifier,
                             Object::ToString(isolate, identifier_obj),
                             JSTemporalTimeZone);
  Handle<String> canonical;
  // 3. If identifier satisfies the syntax of a TimeZoneNumericUTCOffset
  // (see 13.33), then
  bool valid;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, valid,
      IsValidTimeZoneNumericUTCOffsetString(isolate, identifier),
      Handle<JSTemporalTimeZone>());

  if (valid) {
    // a. Let offsetNanoseconds be ? ParseTimeZoneOffsetString(identifier).
    int64_t offset_nanoseconds;
    MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, offset_nanoseconds,
        ParseTimeZoneOffsetString(isolate, identifier),
        Handle<JSTemporalTimeZone>());

    // b. Let canonical be ! FormatTimeZoneOffsetString(offsetNanoseconds).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, canonical,
        FormatTimeZoneOffsetString(isolate, offset_nanoseconds),
        JSTemporalTimeZone);
  } else {
    // 4. Else,
    // a. If ! IsValidTimeZoneName(identifier) is false, then
    if (!IsValidTimeZoneName(isolate, identifier)) {
      // i. Throw a RangeError exception.
      THROW_NEW_ERROR(
          isolate, NewRangeError(MessageTemplate::kInvalidTimeZone, identifier),
          JSTemporalTimeZone);
    }
    // b. Let canonical be ! CanonicalizeTimeZoneName(identifier).
    ASSIGN_RETURN_ON_EXCEPTION(isolate, canonical,
                               CanonicalizeTimeZoneName(isolate, identifier),
                               JSTemporalTimeZone);
  }
  // 5. Return ? CreateTemporalTimeZone(canonical, NewTarget).
  return CreateTemporalTimeZone(isolate, target, new_target, canonical);
}

namespace {

MaybeHandle<JSTemporalPlainDateTime> ToTemporalDateTime(
    Isolate* isolate, Handle<Object> item_obj, Handle<Object> options,
    const char* method_name);

MaybeHandle<JSTemporalPlainDateTime> ToTemporalDateTime(
    Isolate* isolate, Handle<Object> item_obj, const char* method_name) {
  // 1. If options is not present, set options to undefined.
  return ToTemporalDateTime(isolate, item_obj,
                            isolate->factory()->undefined_value(), method_name);
}

}  // namespace

// #sec-temporal.timezone.prototype.getinstantfor
MaybeHandle<JSTemporalInstant> JSTemporalTimeZone::GetInstantFor(
    Isolate* isolate, Handle<JSTemporalTimeZone> time_zone,
    Handle<Object> date_time_obj, Handle<Object> options_obj) {
  const char* method_name = "Temporal.TimeZone.prototype.getInstantFor";
  // 1. Let timeZone be the this value.
  // 2. Perform ? RequireInternalSlot(timeZone,
  // [[InitializedTemporalTimeZone]]).
  // 3. Set dateTime to ? ToTemporalDateTime(dateTime).
  Handle<JSTemporalPlainDateTime> date_time;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, date_time,
      ToTemporalDateTime(isolate, date_time_obj, method_name),
      JSTemporalInstant);

  // 4. Set options to ? GetOptionsObject(options).
  Handle<JSReceiver> options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, GetOptionsObject(isolate, options_obj, method_name),
      JSTemporalInstant);

  // 5. Let disambiguation be ? ToTemporalDisambiguation(options).
  Disambiguation disambiguation;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, disambiguation,
      ToTemporalDisambiguation(isolate, options, method_name),
      Handle<JSTemporalInstant>());

  // 6. Return ? BuiltinTimeZoneGetInstantFor(timeZone, dateTime,
  // disambiguation).
  return BuiltinTimeZoneGetInstantFor(isolate, time_zone, date_time,
                                      disambiguation, method_name);
}

namespace {

// #sec-temporal-getianatimezonenexttransition
MaybeHandle<Object> GetIANATimeZoneNextTransition(Isolate* isolate,
                                                  Handle<BigInt> nanoseconds,
                                                  int32_t time_zone_index) {
#ifdef V8_INTL_SUPPORT
  // TODO(ftang) Add support for non UTC timezone
  DCHECK_EQ(time_zone_index, JSTemporalTimeZone::kUTCTimeZoneIndex);
  return isolate->factory()->null_value();
#else   // V8_INTL_SUPPORT
  return isolate->factory()->null_value();
#endif  // V8_INTL_SUPPORT
}

// #sec-temporal-getianatimezoneprevioustransition
MaybeHandle<Object> GetIANATimeZonePreviousTransition(
    Isolate* isolate, Handle<BigInt> nanoseconds, int32_t time_zone_index) {
#ifdef V8_INTL_SUPPORT
  // TODO(ftang) Add support for non UTC timezone
  DCHECK_EQ(time_zone_index, JSTemporalTimeZone::kUTCTimeZoneIndex);
  return isolate->factory()->null_value();
#else   // V8_INTL_SUPPORT
  return isolate->factory()->null_value();
#endif  // V8_INTL_SUPPORT
}

}  // namespace

// #sec-temporal.timezone.prototype.getplaindatetimefor
MaybeHandle<JSTemporalPlainDateTime> JSTemporalTimeZone::GetPlainDateTimeFor(
    Isolate* isolate, Handle<JSTemporalTimeZone> time_zone,
    Handle<Object> instant_obj, Handle<Object> calendar_like) {
  TEMPORAL_ENTER_FUNC();
  const char* method_name = "Temporal.TimeZone.prototype.getPlainDateTimeFor";
  // 1. 1. Let timeZone be the this value.
  // 2. Perform ? RequireInternalSlot(timeZone,
  // [[InitializedTemporalTimeZone]]).
  // 3. Set instant to ? ToTemporalInstant(instant).
  Handle<JSTemporalInstant> instant;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, instant, ToTemporalInstant(isolate, instant_obj, method_name),
      JSTemporalPlainDateTime);
  // 4. Let calendar be ? ToTemporalCalendarWithISODefault(calendarLike).
  Handle<JSReceiver> calendar;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, calendar,
      ToTemporalCalendarWithISODefault(isolate, calendar_like, method_name),
      JSTemporalPlainDateTime);

  // 5. Return ? BuiltinTimeZoneGetPlainDateTimeFor(timeZone, instant,
  // calendar).
  return temporal::BuiltinTimeZoneGetPlainDateTimeFor(
      isolate, time_zone, instant, calendar, method_name);
}

// template for shared code of Temporal.TimeZone.prototype.getNextTransition and
// Temporal.TimeZone.prototype.getPreviousTransition
template <MaybeHandle<Object> (*iana_func)(Isolate*, Handle<BigInt>, int32_t)>
MaybeHandle<Object> GetTransition(Isolate* isolate,
                                  Handle<JSTemporalTimeZone> time_zone,
                                  Handle<Object> starting_point_obj,
                                  const char* method_name) {
  TEMPORAL_ENTER_FUNC();
  // 1. Let timeZone be the this value.
  // 2. Perform ? RequireInternalSlot(timeZone,
  // [[InitializedTemporalTimeZone]]).
  // 3. Set startingPoint to ? ToTemporalInstant(startingPoint).
  Handle<JSTemporalInstant> starting_point;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, starting_point,
      ToTemporalInstant(isolate, starting_point_obj, method_name), Object);
  // 4. If timeZone.[[OffsetNanoseconds]] is not undefined, return null.
  if (time_zone->is_offset()) {
    return isolate->factory()->null_value();
  }
  // 5. Let transition be ?
  // GetIANATimeZoneNextTransition(startingPoint.[[Nanoseconds]],
  // timeZone.[[Identifier]]).
  Handle<Object> transition_obj;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, transition_obj,
      iana_func(isolate, Handle<BigInt>(starting_point->nanoseconds(), isolate),
                time_zone->time_zone_index()),
      Object);
  // 6. If transition is null, return null.
  if (transition_obj->IsNull()) {
    return isolate->factory()->null_value();
  }
  DCHECK(transition_obj->IsBigInt());
  Handle<BigInt> transition = Handle<BigInt>::cast(transition_obj);
  // 7. Return ! CreateTemporalInstant(transition).
  return temporal::CreateTemporalInstant(isolate, transition);
}

// #sec-temporal.timezone.prototype.getnexttransition
MaybeHandle<Object> JSTemporalTimeZone::GetNextTransition(
    Isolate* isolate, Handle<JSTemporalTimeZone> time_zone,
    Handle<Object> starting_point_obj) {
  return GetTransition<GetIANATimeZoneNextTransition>(
      isolate, time_zone, starting_point_obj,
      "Temporal.TimeZone.prototype.getNextTransition");
}
// #sec-temporal.timezone.prototype.getprevioustransition
MaybeHandle<Object> JSTemporalTimeZone::GetPreviousTransition(
    Isolate* isolate, Handle<JSTemporalTimeZone> time_zone,
    Handle<Object> starting_point_obj) {
  return GetTransition<GetIANATimeZonePreviousTransition>(
      isolate, time_zone, starting_point_obj,
      "Temporal.TimeZone.prototype.getPreviousTransition");
}

// #sec-temporal.timezone.prototype.getpossibleinstantsfor
// #sec-temporal-getianatimezoneepochvalue
MaybeHandle<JSArray> GetIANATimeZoneEpochValueAsArrayOfInstant(
    Isolate* isolate, int32_t time_zone_index,
    const DateTimeRecordCommon& date_time) {
  Factory* factory = isolate->factory();
  // 6. Let possibleInstants be a new empty List.
  Handle<BigInt> epoch_nanoseconds = GetEpochFromISOParts(isolate, date_time);
  Handle<FixedArray> fixed_array;
  if (time_zone_index == JSTemporalTimeZone::kUTCTimeZoneIndex) {
    fixed_array = factory->NewFixedArray(1);
    // 7. For each value epochNanoseconds in possibleEpochNanoseconds, do
    // a. Let instant be ! CreateTemporalInstant(epochNanoseconds).
    Handle<JSTemporalInstant> instant =
        temporal::CreateTemporalInstant(isolate, epoch_nanoseconds)
            .ToHandleChecked();
    // b. Append instant to possibleInstants.
    fixed_array->set(0, *instant);
  } else {
#ifdef V8_INTL_SUPPORT
    // TODO(ftang) put in 402 version of implementation calling intl classes.
    UNIMPLEMENTED();
#else
    UNREACHABLE();
#endif  // V8_INTL_SUPPORT
  }

  // 8. Return ! CreateArrayFromList(possibleInstants).
  return factory->NewJSArrayWithElements(fixed_array);
}

// #sec-temporal.timezone.prototype.getpossibleinstantsfor
MaybeHandle<JSArray> JSTemporalTimeZone::GetPossibleInstantsFor(
    Isolate* isolate, Handle<JSTemporalTimeZone> time_zone,
    Handle<Object> date_time_obj) {
  Factory* factory = isolate->factory();
  // 1. Let timeZone be the this value.
  // 2. Perform ? RequireInternalSlot(timeZone,
  // [[InitializedTemporalTimezone]]).
  // 3. Set dateTime to ? ToTemporalDateTime(dateTime).
  Handle<JSTemporalPlainDateTime> date_time;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, date_time,
      ToTemporalDateTime(isolate, date_time_obj,
                         "Temporal.TimeZone.prototype.getPossibleInstantsFor"),
      JSArray);
  DateTimeRecordCommon date_time_record = {
      {date_time->iso_year(), date_time->iso_month(), date_time->iso_day()},
      {date_time->iso_hour(), date_time->iso_minute(), date_time->iso_second(),
       date_time->iso_millisecond(), date_time->iso_microsecond(),
       date_time->iso_nanosecond()}};
  // 4. If timeZone.[[OffsetNanoseconds]] is not undefined, then
  if (time_zone->is_offset()) {
    // a. Let epochNanoseconds be ! GetEpochFromISOParts(dateTime.[[ISOYear]],
    // dateTime.[[ISOMonth]], dateTime.[[ISODay]], dateTime.[[ISOHour]],
    // dateTime.[[ISOMinute]], dateTime.[[ISOSecond]],
    // dateTime.[[ISOMillisecond]], dateTime.[[ISOMicrosecond]],
    // dateTime.[[ISONanosecond]]).
    Handle<BigInt> epoch_nanoseconds =
        GetEpochFromISOParts(isolate, date_time_record);
    // b. Let instant be ! CreateTemporalInstant(epochNanoseconds −
    // timeZone.[[OffsetNanoseconds]]).
    Handle<JSTemporalInstant> instant =
        temporal::CreateTemporalInstant(
            isolate,
            BigInt::Subtract(
                isolate, epoch_nanoseconds,
                BigInt::FromInt64(isolate, time_zone->offset_nanoseconds()))
                .ToHandleChecked())
            .ToHandleChecked();
    // c. Return ! CreateArrayFromList(« instant »).
    Handle<FixedArray> fixed_array = factory->NewFixedArray(1);
    fixed_array->set(0, *instant);
    return factory->NewJSArrayWithElements(fixed_array);
  }

  // 5. Let possibleEpochNanoseconds be ?
  // GetIANATimeZoneEpochValue(timeZone.[[Identifier]], dateTime.[[ISOYear]],
  // dateTime.[[ISOMonth]], dateTime.[[ISODay]], dateTime.[[ISOHour]],
  // dateTime.[[ISOMinute]], dateTime.[[ISOSecond]],
  // dateTime.[[ISOMillisecond]], dateTime.[[ISOMicrosecond]],
  // dateTime.[[ISONanosecond]]).

  // ... Step 5-8 put into GetIANATimeZoneEpochValueAsArrayOfInstant
  // 8. Return ! CreateArrayFromList(possibleInstants).
  return GetIANATimeZoneEpochValueAsArrayOfInstant(
      isolate, time_zone->time_zone_index(), date_time_record);
}

// #sec-temporal.timezone.prototype.tostring
MaybeHandle<Object> JSTemporalTimeZone::ToString(
    Isolate* isolate, Handle<JSTemporalTimeZone> time_zone,
    const char* method_name) {
  return time_zone->id(isolate);
}

int32_t JSTemporalTimeZone::time_zone_index() const {
  DCHECK(is_offset() == false);
  return offset_milliseconds_or_time_zone_index();
}

int64_t JSTemporalTimeZone::offset_nanoseconds() const {
  TEMPORAL_ENTER_FUNC();
  DCHECK(is_offset());
  return 1000000L * offset_milliseconds() + offset_sub_milliseconds();
}

void JSTemporalTimeZone::set_offset_nanoseconds(int64_t ns) {
  this->set_offset_milliseconds(static_cast<int32_t>(ns / 1000000L));
  this->set_offset_sub_milliseconds(static_cast<int32_t>(ns % 1000000L));
}

MaybeHandle<String> JSTemporalTimeZone::id(Isolate* isolate) const {
  if (is_offset()) {
    return FormatTimeZoneOffsetString(isolate, offset_nanoseconds());
  }
#ifdef V8_INTL_SUPPORT
  std::string id =
      Intl::TimeZoneIdFromIndex(offset_milliseconds_or_time_zone_index());
  return isolate->factory()->NewStringFromAsciiChecked(id.c_str());
#else   // V8_INTL_SUPPORT
  DCHECK_EQ(kUTCTimeZoneIndex, offset_milliseconds_or_time_zone_index());
  return isolate->factory()->UTC_string();
#endif  // V8_INTL_SUPPORT
}

MaybeHandle<JSTemporalPlainDate> JSTemporalPlainDate::Constructor(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    Handle<Object> iso_year_obj, Handle<Object> iso_month_obj,
    Handle<Object> iso_day_obj, Handle<Object> calendar_like) {
  const char* method_name = "Temporal.PlainDate";
  // 1. If NewTarget is undefined, throw a TypeError exception.
  if (new_target->IsUndefined()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     method_name)),
                    JSTemporalPlainDate);
  }
#define TO_INT_THROW_ON_INFTY(name, T)                                        \
  int32_t name;                                                               \
  {                                                                           \
    Handle<Object> number_##name;                                             \
    /* x. Let name be ? ToIntegerThrowOnInfinity(name). */                    \
    ASSIGN_RETURN_ON_EXCEPTION(isolate, number_##name,                        \
                               ToIntegerThrowOnInfinity(isolate, name##_obj), \
                               T);                                            \
    name = NumberToInt32(*number_##name);                                     \
  }

  TO_INT_THROW_ON_INFTY(iso_year, JSTemporalPlainDate);
  TO_INT_THROW_ON_INFTY(iso_month, JSTemporalPlainDate);
  TO_INT_THROW_ON_INFTY(iso_day, JSTemporalPlainDate);

  // 8. Let calendar be ? ToTemporalCalendarWithISODefault(calendarLike).
  Handle<JSReceiver> calendar;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, calendar,
      ToTemporalCalendarWithISODefault(isolate, calendar_like, method_name),
      JSTemporalPlainDate);

  // 9. Return ? CreateTemporalDate(y, m, d, calendar, NewTarget).
  return CreateTemporalDate(isolate, target, new_target,
                            {iso_year, iso_month, iso_day}, calendar);
}

// #sec-temporal.plaindate.prototype.withcalendar
MaybeHandle<JSTemporalPlainDate> JSTemporalPlainDate::WithCalendar(
    Isolate* isolate, Handle<JSTemporalPlainDate> temporal_date,
    Handle<Object> calendar_like) {
  const char* method_name = "Temporal.PlainDate.prototype.withCalendar";
  // 1. Let temporalDate be the this value.
  // 2. Perform ? RequireInternalSlot(temporalDate,
  // [[InitializedTemporalDate]]).
  // 3. Let calendar be ? ToTemporalCalendar(calendar).
  Handle<JSReceiver> calendar;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, calendar,
      temporal::ToTemporalCalendar(isolate, calendar_like, method_name),
      JSTemporalPlainDate);
  // 4. Return ? CreateTemporalDate(temporalDate.[[ISOYear]],
  // temporalDate.[[ISOMonth]], temporalDate.[[ISODay]], calendar).
  return CreateTemporalDate(
      isolate,
      {temporal_date->iso_year(), temporal_date->iso_month(),
       temporal_date->iso_day()},
      calendar);
}

// #sec-temporal.now.plaindate
MaybeHandle<JSTemporalPlainDate> JSTemporalPlainDate::Now(
    Isolate* isolate, Handle<Object> calendar_like,
    Handle<Object> temporal_time_zone_like) {
  const char* method_name = "Temporal.Now.plainDate";
  // 1. Let dateTime be ? SystemDateTime(temporalTimeZoneLike, calendarLike).
  Handle<JSTemporalPlainDateTime> date_time;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, date_time,
                             SystemDateTime(isolate, temporal_time_zone_like,
                                            calendar_like, method_name),
                             JSTemporalPlainDate);
  // 2. Return ! CreateTemporalDate(dateTime.[[ISOYear]], dateTime.[[ISOMonth]],
  // dateTime.[[ISODay]], dateTime.[[Calendar]]).
  return CreateTemporalDate(
      isolate,
      {date_time->iso_year(), date_time->iso_month(), date_time->iso_day()},
      Handle<JSReceiver>(date_time->calendar(), isolate));
}

// #sec-temporal.now.plaindateiso
MaybeHandle<JSTemporalPlainDate> JSTemporalPlainDate::NowISO(
    Isolate* isolate, Handle<Object> temporal_time_zone_like) {
  const char* method_name = "Temporal.Now.plainDateISO";
  // 1. Let calendar be ! GetISO8601Calendar().
  Handle<JSReceiver> calendar;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, calendar,
                             temporal::GetISO8601Calendar(isolate),
                             JSTemporalPlainDate);
  // 2. Let dateTime be ? SystemDateTime(temporalTimeZoneLike, calendar).
  Handle<JSTemporalPlainDateTime> date_time;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, date_time,
      SystemDateTime(isolate, temporal_time_zone_like, calendar, method_name),
      JSTemporalPlainDate);
  // 3. Return ! CreateTemporalDate(dateTime.[[ISOYear]], dateTime.[[ISOMonth]],
  // dateTime.[[ISODay]], dateTime.[[Calendar]]).
  return CreateTemporalDate(
      isolate,
      {date_time->iso_year(), date_time->iso_month(), date_time->iso_day()},
      Handle<JSReceiver>(date_time->calendar(), isolate));
}

// #sec-temporal.plaindate.from
MaybeHandle<JSTemporalPlainDate> JSTemporalPlainDate::From(
    Isolate* isolate, Handle<Object> item, Handle<Object> options_obj) {
  const char* method_name = "Temporal.PlainDate.from";
  // 1. Set options to ? GetOptionsObject(options).
  Handle<JSReceiver> options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, GetOptionsObject(isolate, options_obj, method_name),
      JSTemporalPlainDate);
  // 2. If Type(item) is Object and item has an [[InitializedTemporalDate]]
  // internal slot, then
  if (item->IsJSTemporalPlainDate()) {
    // a. Perform ? ToTemporalOverflow(options).
    ShowOverflow overflow;
    MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, overflow, ToTemporalOverflow(isolate, options, method_name),
        Handle<JSTemporalPlainDate>());
    USE(overflow);
    // b. Return ? CreateTemporalDate(item.[[ISOYear]], item.[[ISOMonth]],
    // item.[[ISODay]], item.[[Calendar]]).
    Handle<JSTemporalPlainDate> date = Handle<JSTemporalPlainDate>::cast(item);
    return CreateTemporalDate(
        isolate, {date->iso_year(), date->iso_month(), date->iso_day()},
        Handle<JSReceiver>(date->calendar(), isolate));
  }
  // 3. Return ? ToTemporalDate(item, options).
  return ToTemporalDate(isolate, item, options, method_name);
}

#define DEFINE_INT_FIELD(obj, str, field, item)                \
  CHECK(JSReceiver::CreateDataProperty(                        \
            isolate, obj, factory->str##_string(),             \
            Handle<Smi>(Smi::FromInt(item->field()), isolate), \
            Just(kThrowOnError))                               \
            .FromJust());

// #sec-temporal.plaindate.prototype.getisofields
MaybeHandle<JSReceiver> JSTemporalPlainDate::GetISOFields(
    Isolate* isolate, Handle<JSTemporalPlainDate> temporal_date) {
  Factory* factory = isolate->factory();
  // 1. Let temporalDate be the this value.
  // 2. Perform ? RequireInternalSlot(temporalDate,
  // [[InitializedTemporalDate]]).
  // 3. Let fields be ! OrdinaryObjectCreate(%Object.prototype%).
  Handle<JSObject> fields =
      isolate->factory()->NewJSObject(isolate->object_function());
  // 4. Perform ! CreateDataPropertyOrThrow(fields, "calendar",
  // temporalDate.[[Calendar]]).
  CHECK(JSReceiver::CreateDataProperty(
            isolate, fields, factory->calendar_string(),
            Handle<JSReceiver>(temporal_date->calendar(), isolate),
            Just(kThrowOnError))
            .FromJust());
  // 5. Perform ! CreateDataPropertyOrThrow(fields, "isoDay",
  // 𝔽(temporalDate.[[ISODay]])).
  // 6. Perform ! CreateDataPropertyOrThrow(fields, "isoMonth",
  // 𝔽(temporalDate.[[ISOMonth]])).
  // 7. Perform ! CreateDataPropertyOrThrow(fields, "isoYear",
  // 𝔽(temporalDate.[[ISOYear]])).
  DEFINE_INT_FIELD(fields, isoDay, iso_day, temporal_date)
  DEFINE_INT_FIELD(fields, isoMonth, iso_month, temporal_date)
  DEFINE_INT_FIELD(fields, isoYear, iso_year, temporal_date)
  // 8. Return fields.
  return fields;
}

// #sec-temporal-createtemporaldatetime
MaybeHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::Constructor(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    Handle<Object> iso_year_obj, Handle<Object> iso_month_obj,
    Handle<Object> iso_day_obj, Handle<Object> hour_obj,
    Handle<Object> minute_obj, Handle<Object> second_obj,
    Handle<Object> millisecond_obj, Handle<Object> microsecond_obj,
    Handle<Object> nanosecond_obj, Handle<Object> calendar_like) {
  const char* method_name = "Temporal.PlainDateTime";
  // 1. If NewTarget is undefined, throw a TypeError exception.
  if (new_target->IsUndefined()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     method_name)),
                    JSTemporalPlainDateTime);
  }

  TO_INT_THROW_ON_INFTY(iso_year, JSTemporalPlainDateTime);
  TO_INT_THROW_ON_INFTY(iso_month, JSTemporalPlainDateTime);
  TO_INT_THROW_ON_INFTY(iso_day, JSTemporalPlainDateTime);
  TO_INT_THROW_ON_INFTY(hour, JSTemporalPlainDateTime);
  TO_INT_THROW_ON_INFTY(minute, JSTemporalPlainDateTime);
  TO_INT_THROW_ON_INFTY(second, JSTemporalPlainDateTime);
  TO_INT_THROW_ON_INFTY(millisecond, JSTemporalPlainDateTime);
  TO_INT_THROW_ON_INFTY(microsecond, JSTemporalPlainDateTime);
  TO_INT_THROW_ON_INFTY(nanosecond, JSTemporalPlainDateTime);

  // 20. Let calendar be ? ToTemporalCalendarWithISODefault(calendarLike).
  Handle<JSReceiver> calendar;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, calendar,
      ToTemporalCalendarWithISODefault(isolate, calendar_like, method_name),
      JSTemporalPlainDateTime);

  // 21. Return ? CreateTemporalDateTime(isoYear, isoMonth, isoDay, hour,
  // minute, second, millisecond, microsecond, nanosecond, calendar, NewTarget).
  return CreateTemporalDateTime(
      isolate, target, new_target,
      {{iso_year, iso_month, iso_day},
       {hour, minute, second, millisecond, microsecond, nanosecond}},
      calendar);
}

namespace {

// #sec-temporal-interprettemporaldatetimefields
Maybe<DateTimeRecord> InterpretTemporalDateTimeFields(
    Isolate* isolate, Handle<JSReceiver> calendar, Handle<JSReceiver> fields,
    Handle<Object> options, const char* method_name) {
  TEMPORAL_ENTER_FUNC();

  // 1. Let timeResult be ? ToTemporalTimeRecord(fields).
  TimeRecord time_result;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, time_result, ToTemporalTimeRecord(isolate, fields, method_name),
      Nothing<DateTimeRecord>());

  // 2. Let temporalDate be ? DateFromFields(calendar, fields, options).
  Handle<JSTemporalPlainDate> temporal_date;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, temporal_date,
      DateFromFields(isolate, calendar, fields, options),
      Nothing<DateTimeRecord>());

  // 3. Let overflow be ? ToTemporalOverflow(options).
  ShowOverflow overflow;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, overflow, ToTemporalOverflow(isolate, options, method_name),
      Nothing<DateTimeRecord>());

  // 4. Let timeResult be ? RegulateTime(timeResult.[[Hour]],
  // timeResult.[[Minute]], timeResult.[[Second]], timeResult.[[Millisecond]],
  // timeResult.[[Microsecond]], timeResult.[[Nanosecond]], overflow).
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, time_result,
      temporal::RegulateTime(isolate, time_result, overflow),
      Nothing<DateTimeRecord>());
  // 5. Return the new Record { [[Year]]: temporalDate.[[ISOYear]], [[Month]]:
  // temporalDate.[[ISOMonth]], [[Day]]: temporalDate.[[ISODay]], [[Hour]]:
  // timeResult.[[Hour]], [[Minute]]: timeResult.[[Minute]], [[Second]]:
  // timeResult.[[Second]], [[Millisecond]]: timeResult.[[Millisecond]],
  // [[Microsecond]]: timeResult.[[Microsecond]], [[Nanosecond]]:
  // timeResult.[[Nanosecond]] }.

  DateTimeRecord result = {
      {temporal_date->iso_year(), temporal_date->iso_month(),
       temporal_date->iso_day()},
      time_result.time,
      Handle<String>()};
  return Just(result);
}

// #sec-temporal-parsetemporaldatetimestring
Maybe<DateTimeRecord> ParseTemporalDateTimeString(Isolate* isolate,
                                                  Handle<String> iso_string) {
  TEMPORAL_ENTER_FUNC();
  // 1. Assert: Type(isoString) is String.
  // 2. If isoString does not satisfy the syntax of a TemporalDateTimeString
  // (see 13.33), then
  base::Optional<ParsedISO8601Result> parsed =
      TemporalParser::ParseTemporalDateTimeString(isolate, iso_string);
  if (!parsed.has_value()) {
    // a. Throw a *RangeError* exception.
    THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                 NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                                 Nothing<DateTimeRecord>());
  }

  // 3. If _isoString_ contains a |UTCDesignator|, then
  if (parsed->utc_designator) {
    // a. Throw a *RangeError* exception.
    THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                 NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                                 Nothing<DateTimeRecord>());
  }

  // 3. Let result be ? ParseISODateTime(isoString).
  // 4. Return result.
  return ParseISODateTime(isolate, iso_string, *parsed);
}

// #sec-temporal-totemporaldatetime
MaybeHandle<JSTemporalPlainDateTime> ToTemporalDateTime(
    Isolate* isolate, Handle<Object> item_obj, Handle<Object> options,
    const char* method_name) {
  TEMPORAL_ENTER_FUNC();
  // 2. Assert: Type(options) is Object or Undefined.
  DCHECK(options->IsJSReceiver() || options->IsUndefined());

  Factory* factory = isolate->factory();
  Handle<JSReceiver> calendar;
  DateTimeRecord result;
  // 2. If Type(item) is Object, then
  if (item_obj->IsJSReceiver()) {
    Handle<JSReceiver> item = Handle<JSReceiver>::cast(item_obj);
    // a. If item has an [[InitializedTemporalDateTime]] internal slot, then
    // i. Return item.
    if (item->IsJSTemporalPlainDateTime()) {
      return Handle<JSTemporalPlainDateTime>::cast(item);
    }
    // b. If item has an [[InitializedTemporalZonedDateTime]] internal slot,
    // then
    if (item->IsJSTemporalZonedDateTime()) {
      // i. Let instant be ! CreateTemporalInstant(item.[[Nanoseconds]]).
      Handle<JSTemporalZonedDateTime> zoned_date_time =
          Handle<JSTemporalZonedDateTime>::cast(item);
      Handle<JSTemporalInstant> instant;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, instant,
          temporal::CreateTemporalInstant(
              isolate, Handle<BigInt>(zoned_date_time->nanoseconds(), isolate)),
          JSTemporalPlainDateTime);
      // ii. Return ?
      // temporal::BuiltinTimeZoneGetPlainDateTimeFor(item.[[TimeZone]],
      // instant, item.[[Calendar]]).
      return temporal::BuiltinTimeZoneGetPlainDateTimeFor(
          isolate, handle(zoned_date_time->time_zone(), isolate), instant,
          handle(zoned_date_time->calendar(), isolate), method_name);
    }
    // c. If item has an [[InitializedTemporalDate]] internal slot, then
    if (item->IsJSTemporalPlainDate()) {
      // i. Return ? CreateTemporalDateTime(item.[[ISOYear]], item.[[ISOMonth]],
      // item.[[ISODay]], 0, 0, 0, 0, 0, 0, item.[[Calendar]]).
      Handle<JSTemporalPlainDate> date =
          Handle<JSTemporalPlainDate>::cast(item);
      return temporal::CreateTemporalDateTime(
          isolate,
          {{date->iso_year(), date->iso_month(), date->iso_day()},
           {0, 0, 0, 0, 0, 0}},
          handle(date->calendar(), isolate));
    }
    // d. Let calendar be ? GetTemporalCalendarWithISODefault(item).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, calendar,
        GetTemporalCalendarWithISODefault(isolate, item, method_name),
        JSTemporalPlainDateTime);
    // e. Let fieldNames be ? CalendarFields(calendar, « "day", "hour",
    // "microsecond", "millisecond", "minute", "month", "monthCode",
    // "nanosecond", "second", "year" »).
    Handle<FixedArray> field_names = factory->NewFixedArray(10);
    field_names->set(0, ReadOnlyRoots(isolate).day_string());
    field_names->set(1, ReadOnlyRoots(isolate).hour_string());
    field_names->set(2, ReadOnlyRoots(isolate).microsecond_string());
    field_names->set(3, ReadOnlyRoots(isolate).millisecond_string());
    field_names->set(4, ReadOnlyRoots(isolate).minute_string());
    field_names->set(5, ReadOnlyRoots(isolate).month_string());
    field_names->set(6, ReadOnlyRoots(isolate).monthCode_string());
    field_names->set(7, ReadOnlyRoots(isolate).nanosecond_string());
    field_names->set(8, ReadOnlyRoots(isolate).second_string());
    field_names->set(9, ReadOnlyRoots(isolate).year_string());
    ASSIGN_RETURN_ON_EXCEPTION(isolate, field_names,
                               CalendarFields(isolate, calendar, field_names),
                               JSTemporalPlainDateTime);
    // f. Let fields be ? PrepareTemporalFields(item,
    // PrepareTemporalFields(item, fieldNames, «»).
    Handle<JSReceiver> fields;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, fields,
                               PrepareTemporalFields(isolate, item, field_names,
                                                     RequiredFields::kNone),
                               JSTemporalPlainDateTime);
    // g. Let result be ?
    // InterpretTemporalDateTimeFields(calendar, fields, options).
    MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, result,
        InterpretTemporalDateTimeFields(isolate, calendar, fields, options,
                                        method_name),
        Handle<JSTemporalPlainDateTime>());
  } else {
    // 3. Else,
    // a. Perform ? ToTemporalOverflow(options).
    ShowOverflow overflow;
    MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, overflow, ToTemporalOverflow(isolate, options, method_name),
        Handle<JSTemporalPlainDateTime>());
    USE(overflow);

    // b. Let string be ? ToString(item).
    Handle<String> string;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, string,
                               Object::ToString(isolate, item_obj),
                               JSTemporalPlainDateTime);
    // c. Let result be ? ParseTemporalDateTimeString(string).
    MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, result, ParseTemporalDateTimeString(isolate, string),
        Handle<JSTemporalPlainDateTime>());
    // d. Assert: ! IsValidISODate(result.[[Year]], result.[[Month]],
    // result.[[Day]]) is true.
    DCHECK(IsValidISODate(isolate, result.date));
    // e. Assert: ! IsValidTime(result.[[Hour]],
    // result.[[Minute]], result.[[Second]], result.[[Millisecond]],
    // result.[[Microsecond]], result.[[Nanosecond]]) is true.
    DCHECK(IsValidTime(isolate, result.time));
    // f. Let calendar
    // be ? ToTemporalCalendarWithISODefault(result.[[Calendar]]).
    Handle<Object> calendar_string;
    if (result.calendar->length() == 0) {
      calendar_string = factory->undefined_value();
    } else {
      calendar_string = result.calendar;
    }
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, calendar,
        ToTemporalCalendarWithISODefault(isolate, calendar_string, method_name),
        JSTemporalPlainDateTime);
  }
  // 4. Return ? CreateTemporalDateTime(result.[[Year]], result.[[Month]],
  // result.[[Day]], result.[[Hour]], result.[[Minute]], result.[[Second]],
  // result.[[Millisecond]], result.[[Microsecond]], result.[[Nanosecond]],
  // calendar).
  return temporal::CreateTemporalDateTime(isolate, {result.date, result.time},
                                          calendar);
}

}  // namespace

// #sec-temporal.plaindatetime.from
MaybeHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::From(
    Isolate* isolate, Handle<Object> item, Handle<Object> options_obj) {
  const char* method_name = "Temporal.PlainDateTime.from";
  // 1. Set options to ? GetOptionsObject(options).
  Handle<JSReceiver> options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, GetOptionsObject(isolate, options_obj, method_name),
      JSTemporalPlainDateTime);
  // 2. If Type(item) is Object and item has an [[InitializedTemporalDateTime]]
  // internal slot, then
  if (item->IsJSTemporalPlainDateTime()) {
    // a. Perform ? ToTemporalOverflow(options).
    ShowOverflow overflow;
    MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, overflow, ToTemporalOverflow(isolate, options, method_name),
        Handle<JSTemporalPlainDateTime>());
    USE(overflow);
    // b. Return ? CreateTemporalDateTime(item.[[ISYear]], item.[[ISOMonth]],
    // item.[[ISODay]], item.[[ISOHour]], item.[[ISOMinute]],
    // item.[[ISOSecond]], item.[[ISOMillisecond]], item.[[ISOMicrosecond]],
    // item.[[ISONanosecond]], item.[[Calendar]]).
    Handle<JSTemporalPlainDateTime> date_time =
        Handle<JSTemporalPlainDateTime>::cast(item);
    return temporal::CreateTemporalDateTime(
        isolate,
        {{date_time->iso_year(), date_time->iso_month(), date_time->iso_day()},
         {date_time->iso_hour(), date_time->iso_minute(),
          date_time->iso_second(), date_time->iso_millisecond(),
          date_time->iso_microsecond(), date_time->iso_nanosecond()}},
        handle(date_time->calendar(), isolate));
  }
  // 3. Return ? ToTemporalDateTime(item, options).
  return ToTemporalDateTime(isolate, item, options, method_name);
}

// #sec-temporal.plaindatetime.prototype.withcalendar
MaybeHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::WithCalendar(
    Isolate* isolate, Handle<JSTemporalPlainDateTime> date_time,
    Handle<Object> calendar_like) {
  const char* method_name = "Temporal.PlainDateTime.prototype.withCalendar";
  // 1. Let temporalDateTime be the this value.
  // 2. Perform ? RequireInternalSlot(temporalDateTime,
  // [[InitializedTemporalDateTime]]).
  // 3. Let calendar be ? ToTemporalCalendar(calendar).
  Handle<JSReceiver> calendar;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, calendar,
      temporal::ToTemporalCalendar(isolate, calendar_like, method_name),
      JSTemporalPlainDateTime);
  // 4. Return ? CreateTemporalDateTime(temporalDateTime.[[ISOYear]],
  // temporalDateTime.[[ISOMonth]], temporalDateTime.[[ISODay]],
  // temporalDateTime.[[ISOHour]], temporalDateTime.[[ISOMinute]],
  // temporalDateTime.[[ISOSecond]], temporalDateTime.[[ISOMillisecond]],
  // temporalDateTime.[[ISOMicrosecond]], temporalDateTime.[[ISONanosecond]],
  // calendar).
  return temporal::CreateTemporalDateTime(
      isolate,
      {{date_time->iso_year(), date_time->iso_month(), date_time->iso_day()},
       {date_time->iso_hour(), date_time->iso_minute(), date_time->iso_second(),
        date_time->iso_millisecond(), date_time->iso_microsecond(),
        date_time->iso_nanosecond()}},
      calendar);
}

// #sec-temporal.now.plaindatetime
MaybeHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::Now(
    Isolate* isolate, Handle<Object> calendar_like,
    Handle<Object> temporal_time_zone_like) {
  const char* method_name = "Temporal.Now.plainDateTime";
  // 1. Return ? SystemDateTime(temporalTimeZoneLike, calendarLike).
  return SystemDateTime(isolate, temporal_time_zone_like, calendar_like,
                        method_name);
}

// #sec-temporal.now.plaindatetimeiso
MaybeHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::NowISO(
    Isolate* isolate, Handle<Object> temporal_time_zone_like) {
  const char* method_name = "Temporal.Now.plainDateTimeISO";
  // 1. Let calendar be ! GetISO8601Calendar().
  Handle<JSReceiver> calendar;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, calendar,
                             temporal::GetISO8601Calendar(isolate),
                             JSTemporalPlainDateTime);
  // 2. Return ? SystemDateTime(temporalTimeZoneLike, calendar).
  return SystemDateTime(isolate, temporal_time_zone_like, calendar,
                        method_name);
}

// #sec-temporal.plaindatetime.prototype.getisofields
MaybeHandle<JSReceiver> JSTemporalPlainDateTime::GetISOFields(
    Isolate* isolate, Handle<JSTemporalPlainDateTime> date_time) {
  Factory* factory = isolate->factory();
  // 1. Let dateTime be the this value.
  // 2. Perform ? RequireInternalSlot(temporalDateTime,
  // [[InitializedTemporalDateTime]]).
  // 3. Let fields be ! OrdinaryObjectCreate(%Object.prototype%).
  Handle<JSObject> fields =
      isolate->factory()->NewJSObject(isolate->object_function());
  // 4. Perform ! CreateDataPropertyOrThrow(fields, "calendar",
  // temporalTime.[[Calendar]]).
  CHECK(JSReceiver::CreateDataProperty(
            isolate, fields, factory->calendar_string(),
            Handle<JSReceiver>(date_time->calendar(), isolate),
            Just(kThrowOnError))
            .FromJust());
  // 5. Perform ! CreateDataPropertyOrThrow(fields, "isoDay",
  // 𝔽(dateTime.[[ISODay]])).
  // 6. Perform ! CreateDataPropertyOrThrow(fields, "isoHour",
  // 𝔽(temporalTime.[[ISOHour]])).
  // 7. Perform ! CreateDataPropertyOrThrow(fields, "isoMicrosecond",
  // 𝔽(temporalTime.[[ISOMicrosecond]])).
  // 8. Perform ! CreateDataPropertyOrThrow(fields, "isoMillisecond",
  // 𝔽(temporalTime.[[ISOMillisecond]])).
  // 9. Perform ! CreateDataPropertyOrThrow(fields, "isoMinute",
  // 𝔽(temporalTime.[[ISOMinute]])).
  // 10. Perform ! CreateDataPropertyOrThrow(fields, "isoMonth",
  // 𝔽(temporalTime.[[ISOMonth]])).
  // 11. Perform ! CreateDataPropertyOrThrow(fields, "isoNanosecond",
  // 𝔽(temporalTime.[[ISONanosecond]])).
  // 12. Perform ! CreateDataPropertyOrThrow(fields, "isoSecond",
  // 𝔽(temporalTime.[[ISOSecond]])).
  // 13. Perform ! CreateDataPropertyOrThrow(fields, "isoYear",
  // 𝔽(temporalTime.[[ISOYear]])).
  DEFINE_INT_FIELD(fields, isoDay, iso_day, date_time)
  DEFINE_INT_FIELD(fields, isoHour, iso_hour, date_time)
  DEFINE_INT_FIELD(fields, isoMicrosecond, iso_microsecond, date_time)
  DEFINE_INT_FIELD(fields, isoMillisecond, iso_millisecond, date_time)
  DEFINE_INT_FIELD(fields, isoMinute, iso_minute, date_time)
  DEFINE_INT_FIELD(fields, isoMonth, iso_month, date_time)
  DEFINE_INT_FIELD(fields, isoNanosecond, iso_nanosecond, date_time)
  DEFINE_INT_FIELD(fields, isoSecond, iso_second, date_time)
  DEFINE_INT_FIELD(fields, isoYear, iso_year, date_time)
  // 14. Return fields.
  return fields;
}

// #sec-temporal.plainmonthday
MaybeHandle<JSTemporalPlainMonthDay> JSTemporalPlainMonthDay::Constructor(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    Handle<Object> iso_month_obj, Handle<Object> iso_day_obj,
    Handle<Object> calendar_like, Handle<Object> reference_iso_year_obj) {
  const char* method_name = "Temporal.PlainMonthDay";
  // 1. If NewTarget is undefined, throw a TypeError exception.
  if (new_target->IsUndefined()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     method_name)),
                    JSTemporalPlainMonthDay);
  }

  // 3. Let m be ? ToIntegerThrowOnInfinity(isoMonth).
  TO_INT_THROW_ON_INFTY(iso_month, JSTemporalPlainMonthDay);
  // 5. Let d be ? ToIntegerThrowOnInfinity(isoDay).
  TO_INT_THROW_ON_INFTY(iso_day, JSTemporalPlainMonthDay);
  // 7. Let calendar be ? ToTemporalCalendarWithISODefault(calendarLike).
  Handle<JSReceiver> calendar;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, calendar,
      ToTemporalCalendarWithISODefault(isolate, calendar_like, method_name),
      JSTemporalPlainMonthDay);

  // 2. If referenceISOYear is undefined, then
  // a. Set referenceISOYear to 1972𝔽.
  // ...
  // 8. Let ref be ? ToIntegerThrowOnInfinity(referenceISOYear).
  int32_t ref = 1972;
  if (!reference_iso_year_obj->IsUndefined()) {
    TO_INT_THROW_ON_INFTY(reference_iso_year, JSTemporalPlainMonthDay);
    ref = reference_iso_year;
  }

  // 10. Return ? CreateTemporalMonthDay(y, m, calendar, ref, NewTarget).
  return CreateTemporalMonthDay(isolate, target, new_target, iso_month, iso_day,
                                calendar, ref);
}

// #sec-temporal.plainmonthday.prototype.getisofields
MaybeHandle<JSReceiver> JSTemporalPlainMonthDay::GetISOFields(
    Isolate* isolate, Handle<JSTemporalPlainMonthDay> month_day) {
  Factory* factory = isolate->factory();
  // 1. Let monthDay be the this value.
  // 2. Perform ? RequireInternalSlot(monthDay,
  // [[InitializedTemporalMonthDay]]).
  // 3. Let fields be ! OrdinaryObjectCreate(%Object.prototype%).
  Handle<JSObject> fields = factory->NewJSObject(isolate->object_function());
  // 4. Perform ! CreateDataPropertyOrThrow(fields, "calendar",
  // montyDay.[[Calendar]]).
  CHECK(JSReceiver::CreateDataProperty(
            isolate, fields, factory->calendar_string(),
            Handle<JSReceiver>(month_day->calendar(), isolate),
            Just(kThrowOnError))
            .FromJust());

  // 5. Perform ! CreateDataPropertyOrThrow(fields, "isoDay",
  // 𝔽(montyDay.[[ISODay]])).
  // 6. Perform ! CreateDataPropertyOrThrow(fields, "isoMonth",
  // 𝔽(montyDay.[[ISOMonth]])).
  // 7. Perform ! CreateDataPropertyOrThrow(fields, "isoYear",
  // 𝔽(montyDay.[[ISOYear]])).
  DEFINE_INT_FIELD(fields, isoDay, iso_day, month_day)
  DEFINE_INT_FIELD(fields, isoMonth, iso_month, month_day)
  DEFINE_INT_FIELD(fields, isoYear, iso_year, month_day)
  // 8. Return fields.
  return fields;
}

MaybeHandle<JSTemporalPlainYearMonth> JSTemporalPlainYearMonth::Constructor(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    Handle<Object> iso_year_obj, Handle<Object> iso_month_obj,
    Handle<Object> calendar_like, Handle<Object> reference_iso_day_obj) {
  const char* method_name = "Temporal.PlainYearMonth";
  // 1. If NewTarget is undefined, throw a TypeError exception.
  if (new_target->IsUndefined()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     method_name)),
                    JSTemporalPlainYearMonth);
  }
  // 7. Let calendar be ? ToTemporalCalendarWithISODefault(calendarLike).
  // 10. Return ? CreateTemporalYearMonth(y, m, calendar, ref, NewTarget).

  // 3. Let y be ? ToIntegerThrowOnInfinity(isoYear).
  TO_INT_THROW_ON_INFTY(iso_year, JSTemporalPlainYearMonth);
  // 5. Let m be ? ToIntegerThrowOnInfinity(isoMonth).
  TO_INT_THROW_ON_INFTY(iso_month, JSTemporalPlainYearMonth);
  // 7. Let calendar be ? ToTemporalCalendarWithISODefault(calendarLike).
  Handle<JSReceiver> calendar;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, calendar,
      ToTemporalCalendarWithISODefault(isolate, calendar_like, method_name),
      JSTemporalPlainYearMonth);

  // 2. If referenceISODay is undefined, then
  // a. Set referenceISODay to 1𝔽.
  // ...
  // 8. Let ref be ? ToIntegerThrowOnInfinity(referenceISODay).
  int32_t ref = 1;
  if (!reference_iso_day_obj->IsUndefined()) {
    TO_INT_THROW_ON_INFTY(reference_iso_day, JSTemporalPlainYearMonth);
    ref = reference_iso_day;
  }

  // 10. Return ? CreateTemporalYearMonth(y, m, calendar, ref, NewTarget).
  return CreateTemporalYearMonth(isolate, target, new_target, iso_year,
                                 iso_month, calendar, ref);
}

// #sec-temporal.plainyearmonth.prototype.getisofields
MaybeHandle<JSReceiver> JSTemporalPlainYearMonth::GetISOFields(
    Isolate* isolate, Handle<JSTemporalPlainYearMonth> year_month) {
  Factory* factory = isolate->factory();
  // 1. Let yearMonth be the this value.
  // 2. Perform ? RequireInternalSlot(yearMonth,
  // [[InitializedTemporalYearMonth]]).
  // 3. Let fields be ! OrdinaryObjectCreate(%Object.prototype%).
  Handle<JSObject> fields =
      isolate->factory()->NewJSObject(isolate->object_function());
  // 4. Perform ! CreateDataPropertyOrThrow(fields, "calendar",
  // yearMonth.[[Calendar]]).
  CHECK(JSReceiver::CreateDataProperty(
            isolate, fields, factory->calendar_string(),
            Handle<JSReceiver>(year_month->calendar(), isolate),
            Just(kThrowOnError))
            .FromJust());
  // 5. Perform ! CreateDataPropertyOrThrow(fields, "isoDay",
  // 𝔽(yearMonth.[[ISODay]])).
  // 6. Perform ! CreateDataPropertyOrThrow(fields, "isoMonth",
  // 𝔽(yearMonth.[[ISOMonth]])).
  // 7. Perform ! CreateDataPropertyOrThrow(fields, "isoYear",
  // 𝔽(yearMonth.[[ISOYear]])).
  DEFINE_INT_FIELD(fields, isoDay, iso_day, year_month)
  DEFINE_INT_FIELD(fields, isoMonth, iso_month, year_month)
  DEFINE_INT_FIELD(fields, isoYear, iso_year, year_month)
  // 8. Return fields.
  return fields;
}

// #sec-temporal-plaintime-constructor
MaybeHandle<JSTemporalPlainTime> JSTemporalPlainTime::Constructor(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    Handle<Object> hour_obj, Handle<Object> minute_obj,
    Handle<Object> second_obj, Handle<Object> millisecond_obj,
    Handle<Object> microsecond_obj, Handle<Object> nanosecond_obj) {
  const char* method_name = "Temporal.PlainTime";
  // 1. If NewTarget is undefined, then
  // a. Throw a TypeError exception.
  if (new_target->IsUndefined()) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     method_name)),
                    JSTemporalPlainTime);
  }

  TO_INT_THROW_ON_INFTY(hour, JSTemporalPlainTime);
  TO_INT_THROW_ON_INFTY(minute, JSTemporalPlainTime);
  TO_INT_THROW_ON_INFTY(second, JSTemporalPlainTime);
  TO_INT_THROW_ON_INFTY(millisecond, JSTemporalPlainTime);
  TO_INT_THROW_ON_INFTY(microsecond, JSTemporalPlainTime);
  TO_INT_THROW_ON_INFTY(nanosecond, JSTemporalPlainTime);

  // 14. Return ? CreateTemporalTime(hour, minute, second, millisecond,
  // microsecond, nanosecond, NewTarget).
  return CreateTemporalTime(
      isolate, target, new_target,
      {hour, minute, second, millisecond, microsecond, nanosecond});
}

// #sec-temporal.now.plaintimeiso
MaybeHandle<JSTemporalPlainTime> JSTemporalPlainTime::NowISO(
    Isolate* isolate, Handle<Object> temporal_time_zone_like) {
  const char* method_name = "Temporal.Now.plainTimeISO";
  // 1. Let calendar be ! GetISO8601Calendar().
  Handle<JSReceiver> calendar;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, calendar,
                             temporal::GetISO8601Calendar(isolate),
                             JSTemporalPlainTime);
  // 2. Let dateTime be ? SystemDateTime(temporalTimeZoneLike, calendar).
  Handle<JSTemporalPlainDateTime> date_time;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, date_time,
      SystemDateTime(isolate, temporal_time_zone_like, calendar, method_name),
      JSTemporalPlainTime);
  // 3. Return ! CreateTemporalTime(dateTime.[[ISOHour]],
  // dateTime.[[ISOMinute]], dateTime.[[ISOSecond]],
  // dateTime.[[ISOMillisecond]], dateTime.[[ISOMicrosecond]],
  // dateTime.[[ISONanosecond]]).
  return CreateTemporalTime(
      isolate, {date_time->iso_hour(), date_time->iso_minute(),
                date_time->iso_second(), date_time->iso_millisecond(),
                date_time->iso_microsecond(), date_time->iso_nanosecond()});
}

// #sec-temporal.plaintime.from
MaybeHandle<JSTemporalPlainTime> JSTemporalPlainTime::From(
    Isolate* isolate, Handle<Object> item_obj, Handle<Object> options_obj) {
  const char* method_name = "Temporal.PlainTime.from";
  // 1. Set options to ? GetOptionsObject(options).
  Handle<JSReceiver> options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, GetOptionsObject(isolate, options_obj, method_name),
      JSTemporalPlainTime);
  // 2. Let overflow be ? ToTemporalOverflow(options).
  ShowOverflow overflow;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, overflow, ToTemporalOverflow(isolate, options, method_name),
      Handle<JSTemporalPlainTime>());
  // 3. If Type(item) is Object and item has an [[InitializedTemporalTime]]
  // internal slot, then
  if (item_obj->IsJSTemporalPlainTime()) {
    // a. Return ? CreateTemporalTime(item.[[ISOHour]], item.[[ISOMinute]],
    // item.[[ISOSecond]], item.[[ISOMillisecond]], item.[[ISOMicrosecond]],
    // item.[[ISONanosecond]]).
    Handle<JSTemporalPlainTime> item =
        Handle<JSTemporalPlainTime>::cast(item_obj);
    return CreateTemporalTime(
        isolate, {item->iso_hour(), item->iso_minute(), item->iso_second(),
                  item->iso_millisecond(), item->iso_microsecond(),
                  item->iso_nanosecond()});
  }
  // 4. Return ? ToTemporalTime(item, overflow).
  return temporal::ToTemporalTime(isolate, item_obj, overflow, method_name);
}

// #sec-temporal.plaintime.prototype.getisofields
MaybeHandle<JSReceiver> JSTemporalPlainTime::GetISOFields(
    Isolate* isolate, Handle<JSTemporalPlainTime> temporal_time) {
  Factory* factory = isolate->factory();
  // 1. Let temporalTime be the this value.
  // 2. Perform ? RequireInternalSlot(temporalTime,
  // [[InitializedTemporalTime]]).
  // 3. Let fields be ! OrdinaryObjectCreate(%Object.prototype%).
  Handle<JSObject> fields =
      isolate->factory()->NewJSObject(isolate->object_function());
  // 4. Perform ! CreateDataPropertyOrThrow(fields, "calendar",
  // temporalTime.[[Calendar]]).
  Handle<JSTemporalCalendar> iso8601_calendar;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, iso8601_calendar,
                             temporal::GetISO8601Calendar(isolate),
                             JSTemporalPlainTime);
  CHECK(JSReceiver::CreateDataProperty(isolate, fields,
                                       factory->calendar_string(),
                                       iso8601_calendar, Just(kThrowOnError))
            .FromJust());

  // 5. Perform ! CreateDataPropertyOrThrow(fields, "isoHour",
  // 𝔽(temporalTime.[[ISOHour]])).
  // 6. Perform ! CreateDataPropertyOrThrow(fields, "isoMicrosecond",
  // 𝔽(temporalTime.[[ISOMicrosecond]])).
  // 7. Perform ! CreateDataPropertyOrThrow(fields, "isoMillisecond",
  // 𝔽(temporalTime.[[ISOMillisecond]])).
  // 8. Perform ! CreateDataPropertyOrThrow(fields, "isoMinute",
  // 𝔽(temporalTime.[[ISOMinute]])).
  // 9. Perform ! CreateDataPropertyOrThrow(fields, "isoNanosecond",
  // 𝔽(temporalTime.[[ISONanosecond]])).
  // 10. Perform ! CreateDataPropertyOrThrow(fields, "isoSecond",
  // 𝔽(temporalTime.[[ISOSecond]])).
  DEFINE_INT_FIELD(fields, isoHour, iso_hour, temporal_time)
  DEFINE_INT_FIELD(fields, isoMicrosecond, iso_microsecond, temporal_time)
  DEFINE_INT_FIELD(fields, isoMillisecond, iso_millisecond, temporal_time)
  DEFINE_INT_FIELD(fields, isoMinute, iso_minute, temporal_time)
  DEFINE_INT_FIELD(fields, isoNanosecond, iso_nanosecond, temporal_time)
  DEFINE_INT_FIELD(fields, isoSecond, iso_second, temporal_time)
  // 11. Return fields.
  return fields;
}

// #sec-temporal.zoneddatetime
MaybeHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::Constructor(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    Handle<Object> epoch_nanoseconds_obj, Handle<Object> time_zone_like,
    Handle<Object> calendar_like) {
  const char* method_name = "Temporal.ZonedDateTime";
  // 1. If NewTarget is undefined, then
  if (new_target->IsUndefined()) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     method_name)),
                    JSTemporalZonedDateTime);
  }
  // 2. Set epochNanoseconds to ? ToBigInt(epochNanoseconds).
  Handle<BigInt> epoch_nanoseconds;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, epoch_nanoseconds,
                             BigInt::FromObject(isolate, epoch_nanoseconds_obj),
                             JSTemporalZonedDateTime);
  // 3. If ! IsValidEpochNanoseconds(epochNanoseconds) is false, throw a
  // RangeError exception.
  if (!IsValidEpochNanoseconds(isolate, epoch_nanoseconds)) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                    JSTemporalZonedDateTime);
  }

  // 4. Let timeZone be ? ToTemporalTimeZone(timeZoneLike).
  Handle<JSReceiver> time_zone;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, time_zone,
      temporal::ToTemporalTimeZone(isolate, time_zone_like, method_name),
      JSTemporalZonedDateTime);

  // 5. Let calendar be ? ToTemporalCalendarWithISODefault(calendarLike).
  Handle<JSReceiver> calendar;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, calendar,
      ToTemporalCalendarWithISODefault(isolate, calendar_like, method_name),
      JSTemporalZonedDateTime);

  // 6. Return ? CreateTemporalZonedDateTime(epochNanoseconds, timeZone,
  // calendar, NewTarget).
  return CreateTemporalZonedDateTime(isolate, target, new_target,
                                     epoch_nanoseconds, time_zone, calendar);
}

// #sec-temporal.zoneddatetime.prototype.withcalendar
MaybeHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::WithCalendar(
    Isolate* isolate, Handle<JSTemporalZonedDateTime> zoned_date_time,
    Handle<Object> calendar_like) {
  TEMPORAL_ENTER_FUNC();
  const char* method_name = "Temporal.ZonedDateTime.prototype.withCalendar";
  // 1. Let zonedDateTime be the this value.
  // 2. Perform ? RequireInternalSlot(zonedDateTime,
  // [[InitializedTemporalZonedDateTime]]).
  // 3. Let calendar be ? ToTemporalCalendar(calendarLike).
  Handle<JSReceiver> calendar;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, calendar,
      temporal::ToTemporalCalendar(isolate, calendar_like, method_name),
      JSTemporalZonedDateTime);

  // 4. Return ? CreateTemporalZonedDateTime(zonedDateTime.[[Nanoseconds]],
  // zonedDateTime.[[TimeZone]], calendar).
  Handle<BigInt> nanoseconds = handle(zoned_date_time->nanoseconds(), isolate);
  Handle<JSReceiver> time_zone = handle(zoned_date_time->time_zone(), isolate);
  return CreateTemporalZonedDateTime(isolate, nanoseconds, time_zone, calendar);
}

// #sec-temporal.zoneddatetime.prototype.withtimezone
MaybeHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::WithTimeZone(
    Isolate* isolate, Handle<JSTemporalZonedDateTime> zoned_date_time,
    Handle<Object> time_zone_like) {
  TEMPORAL_ENTER_FUNC();
  const char* method_name = "Temporal.ZonedDateTime.prototype.withTimeZone";
  // 1. Let zonedDateTime be the this value.
  // 2. Perform ? RequireInternalSlot(zonedDateTime,
  // [[InitializedTemporalZonedDateTime]]).
  // 3. Let timeZone be ? ToTemporalTimeZone(timeZoneLike).
  Handle<JSReceiver> time_zone;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, time_zone,
      temporal::ToTemporalTimeZone(isolate, time_zone_like, method_name),
      JSTemporalZonedDateTime);

  // 4. Return ? CreateTemporalZonedDateTime(zonedDateTime.[[Nanoseconds]],
  // timeZone, zonedDateTime.[[Calendar]]).
  Handle<BigInt> nanoseconds = handle(zoned_date_time->nanoseconds(), isolate);
  Handle<JSReceiver> calendar = handle(zoned_date_time->calendar(), isolate);
  return CreateTemporalZonedDateTime(isolate, nanoseconds, time_zone, calendar);
}

// Common code shared by ZonedDateTime.prototype.toPlainYearMonth and
// toPlainMonthDay
template <typename T,
          MaybeHandle<T> (*from_fields_func)(
              Isolate*, Handle<JSReceiver>, Handle<JSReceiver>, Handle<Object>)>
MaybeHandle<T> ZonedDateTimeToPlainYearMonthOrMonthDay(
    Isolate* isolate, Handle<JSTemporalZonedDateTime> zoned_date_time,
    Handle<String> field_name_1, Handle<String> field_name_2,
    const char* method_name) {
  TEMPORAL_ENTER_FUNC();
  Factory* factory = isolate->factory();
  // 1. Let zonedDateTime be the this value.
  // 2. Perform ? RequireInternalSlot(zonedDateTime,
  // [[InitializedTemporalZonedDateTime]]).
  // 3. Let timeZone be zonedDateTime.[[TimeZone]].
  Handle<JSReceiver> time_zone = handle(zoned_date_time->time_zone(), isolate);
  // 4. Let instant be ! CreateTemporalInstant(zonedDateTime.[[Nanoseconds]]).
  Handle<JSTemporalInstant> instant;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, instant,
      temporal::CreateTemporalInstant(
          isolate, Handle<BigInt>(zoned_date_time->nanoseconds(), isolate)),
      T);
  // 5. Let calendar be zonedDateTime.[[Calendar]].
  Handle<JSReceiver> calendar = handle(zoned_date_time->calendar(), isolate);
  // 6. Let temporalDateTime be ?
  // temporal::BuiltinTimeZoneGetPlainDateTimeFor(timeZone, instant, calendar).
  Handle<JSTemporalPlainDateTime> temporal_date_time;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, temporal_date_time,
      temporal::BuiltinTimeZoneGetPlainDateTimeFor(isolate, time_zone, instant,
                                                   calendar, method_name),
      T);
  // 7. Let fieldNames be ? CalendarFields(calendar, « field_name_1,
  // field_name_2 »).
  Handle<FixedArray> field_names = factory->NewFixedArray(2);
  field_names->set(0, *field_name_1);
  field_names->set(1, *field_name_2);
  ASSIGN_RETURN_ON_EXCEPTION(isolate, field_names,
                             CalendarFields(isolate, calendar, field_names), T);
  // 8. Let fields be ? PrepareTemporalFields(temporalDateTime, fieldNames, «»).
  Handle<JSReceiver> fields;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, fields,
      PrepareTemporalFields(isolate, temporal_date_time, field_names,
                            RequiredFields::kNone),
      T);
  // 9. Return ? XxxFromFields(calendar, fields).
  return from_fields_func(isolate, calendar, fields,
                          factory->undefined_value());
}

// #sec-temporal.zoneddatetime.prototype.toplainyearmonth
MaybeHandle<JSTemporalPlainYearMonth> JSTemporalZonedDateTime::ToPlainYearMonth(
    Isolate* isolate, Handle<JSTemporalZonedDateTime> zoned_date_time) {
  return ZonedDateTimeToPlainYearMonthOrMonthDay<JSTemporalPlainYearMonth,
                                                 YearMonthFromFields>(
      isolate, zoned_date_time, isolate->factory()->monthCode_string(),
      isolate->factory()->year_string(),
      "Temporal.ZonedDateTime.prototype.toPlainYearMonth");
}

// #sec-temporal.zoneddatetime.prototype.toplainmonthday
MaybeHandle<JSTemporalPlainMonthDay> JSTemporalZonedDateTime::ToPlainMonthDay(
    Isolate* isolate, Handle<JSTemporalZonedDateTime> zoned_date_time) {
  return ZonedDateTimeToPlainYearMonthOrMonthDay<JSTemporalPlainMonthDay,
                                                 MonthDayFromFields>(
      isolate, zoned_date_time, isolate->factory()->day_string(),
      isolate->factory()->monthCode_string(),
      "Temporal.ZonedDateTime.prototype.toPlainMonthDay");
}

// #sec-temporal.now.zoneddatetime
MaybeHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::Now(
    Isolate* isolate, Handle<Object> calendar_like,
    Handle<Object> temporal_time_zone_like) {
  const char* method_name = "Temporal.Now.zonedDateTime";
  // 1. Return ? SystemZonedDateTime(temporalTimeZoneLike, calendarLike).
  return SystemZonedDateTime(isolate, temporal_time_zone_like, calendar_like,
                             method_name);
}

// #sec-temporal.now.zoneddatetimeiso
MaybeHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::NowISO(
    Isolate* isolate, Handle<Object> temporal_time_zone_like) {
  TEMPORAL_ENTER_FUNC();
  const char* method_name = "Temporal.Now.zonedDateTimeISO";
  // 1. Let calendar be ! GetISO8601Calendar().
  Handle<JSReceiver> calendar;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, calendar,
                             temporal::GetISO8601Calendar(isolate),
                             JSTemporalZonedDateTime);
  // 2. Return ? SystemZonedDateTime(temporalTimeZoneLike, calendar).
  return SystemZonedDateTime(isolate, temporal_time_zone_like, calendar,
                             method_name);
}

// #sec-temporal.zoneddatetime.prototype.getisofields
MaybeHandle<JSReceiver> JSTemporalZonedDateTime::GetISOFields(
    Isolate* isolate, Handle<JSTemporalZonedDateTime> zoned_date_time) {
  TEMPORAL_ENTER_FUNC();
  const char* method_name = "Temporal.ZonedDateTime.prototype.getISOFields";
  Factory* factory = isolate->factory();
  // 1. Let zonedDateTime be the this value.
  // 2. Perform ? RequireInternalSlot(zonedDateTime,
  // [[InitializedTemporalZonedDateTime]]).
  // 3. Let fields be ! OrdinaryObjectCreate(%Object.prototype%).
  Handle<JSObject> fields =
      isolate->factory()->NewJSObject(isolate->object_function());
  // 4. Let timeZone be zonedDateTime.[[TimeZone]].
  Handle<JSReceiver> time_zone =
      Handle<JSReceiver>(zoned_date_time->time_zone(), isolate);
  // 5. Let instant be ? CreateTemporalInstant(zonedDateTime.[[Nanoseconds]]).
  Handle<JSTemporalInstant> instant;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, instant,
      temporal::CreateTemporalInstant(
          isolate, Handle<BigInt>(zoned_date_time->nanoseconds(), isolate)),
      JSReceiver);

  // 6. Let calendar be zonedDateTime.[[Calendar]].
  Handle<JSReceiver> calendar =
      Handle<JSReceiver>(zoned_date_time->calendar(), isolate);
  // 7. Let dateTime be ? BuiltinTimeZoneGetPlainDateTimeFor(timeZone,
  // instant, calendar).
  Handle<JSTemporalPlainDateTime> date_time;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, date_time,
      temporal::BuiltinTimeZoneGetPlainDateTimeFor(isolate, time_zone, instant,
                                                   calendar, method_name),
      JSReceiver);
  // 8. Let offset be ? BuiltinTimeZoneGetOffsetStringFor(timeZone, instant).
  Handle<String> offset;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, offset,
                             BuiltinTimeZoneGetOffsetStringFor(
                                 isolate, time_zone, instant, method_name),
                             JSReceiver);

#define DEFINE_STRING_FIELD(obj, str, field)                                  \
  CHECK(JSReceiver::CreateDataProperty(isolate, obj, factory->str##_string(), \
                                       field, Just(kThrowOnError))            \
            .FromJust());

  // 9. Perform ! CreateDataPropertyOrThrow(fields, "calendar", calendar).
  // 10. Perform ! CreateDataPropertyOrThrow(fields, "isoDay",
  // 𝔽(dateTime.[[ISODay]])).
  // 11. Perform ! CreateDataPropertyOrThrow(fields, "isoHour",
  // 𝔽(temporalTime.[[ISOHour]])).
  // 12. Perform ! CreateDataPropertyOrThrow(fields, "isoMicrosecond",
  // 𝔽(temporalTime.[[ISOMicrosecond]])).
  // 13. Perform ! CreateDataPropertyOrThrow(fields, "isoMillisecond",
  // 𝔽(temporalTime.[[ISOMillisecond]])).
  // 14. Perform ! CreateDataPropertyOrThrow(fields, "isoMinute",
  // 𝔽(temporalTime.[[ISOMinute]])).
  // 15. Perform ! CreateDataPropertyOrThrow(fields, "isoMonth",
  // 𝔽(temporalTime.[[ISOMonth]])).
  // 16. Perform ! CreateDataPropertyOrThrow(fields, "isoNanosecond",
  // 𝔽(temporalTime.[[ISONanosecond]])).
  // 17. Perform ! CreateDataPropertyOrThrow(fields, "isoSecond",
  // 𝔽(temporalTime.[[ISOSecond]])).
  // 18. Perform ! CreateDataPropertyOrThrow(fields, "isoYear",
  // 𝔽(temporalTime.[[ISOYear]])).
  // 19. Perform ! CreateDataPropertyOrThrow(fields, "offset", offset).
  // 20. Perform ! CreateDataPropertyOrThrow(fields, "timeZone", timeZone).
  DEFINE_STRING_FIELD(fields, calendar, calendar)
  DEFINE_INT_FIELD(fields, isoDay, iso_day, date_time)
  DEFINE_INT_FIELD(fields, isoHour, iso_hour, date_time)
  DEFINE_INT_FIELD(fields, isoMicrosecond, iso_microsecond, date_time)
  DEFINE_INT_FIELD(fields, isoMillisecond, iso_millisecond, date_time)
  DEFINE_INT_FIELD(fields, isoMinute, iso_minute, date_time)
  DEFINE_INT_FIELD(fields, isoMonth, iso_month, date_time)
  DEFINE_INT_FIELD(fields, isoNanosecond, iso_nanosecond, date_time)
  DEFINE_INT_FIELD(fields, isoSecond, iso_second, date_time)
  DEFINE_INT_FIELD(fields, isoYear, iso_year, date_time)
  DEFINE_STRING_FIELD(fields, offset, offset)
  DEFINE_STRING_FIELD(fields, timeZone, time_zone)
  // 21. Return fields.
  return fields;
}

// #sec-temporal.now.instant
MaybeHandle<JSTemporalInstant> JSTemporalInstant::Now(Isolate* isolate) {
  TEMPORAL_ENTER_FUNC();
  return SystemInstant(isolate);
}

// #sec-get-temporal.zoneddatetime.prototype.offsetnanoseconds
MaybeHandle<Object> JSTemporalZonedDateTime::OffsetNanoseconds(
    Isolate* isolate, Handle<JSTemporalZonedDateTime> zoned_date_time) {
  TEMPORAL_ENTER_FUNC();
  // 1. Let zonedDateTime be the this value.
  // 2. Perform ? RequireInternalSlot(zonedDateTime,
  // [[InitializedTemporalZonedDateTime]]).
  // 3. Let timeZone be zonedDateTime.[[TimeZone]].
  Handle<JSReceiver> time_zone = handle(zoned_date_time->time_zone(), isolate);
  // 4. Let instant be ! CreateTemporalInstant(zonedDateTime.[[Nanoseconds]]).
  Handle<JSTemporalInstant> instant;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, instant,
      temporal::CreateTemporalInstant(
          isolate, handle(zoned_date_time->nanoseconds(), isolate)),
      Object);
  // 5. Return 𝔽(? GetOffsetNanosecondsFor(timeZone, instant)).
  int64_t result;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, result,
      GetOffsetNanosecondsFor(
          isolate, time_zone, instant,
          "Temporal.ZonedDateTime.prototype.offsetNanoseconds"),
      Handle<Object>());
  return isolate->factory()->NewNumberFromInt64(result);
}

// #sec-get-temporal.zoneddatetime.prototype.offset
MaybeHandle<String> JSTemporalZonedDateTime::Offset(
    Isolate* isolate, Handle<JSTemporalZonedDateTime> zoned_date_time) {
  TEMPORAL_ENTER_FUNC();
  // 1. Let zonedDateTime be the this value.
  // 2. Perform ? RequireInternalSlot(zonedDateTime,
  // [[InitializedTemporalZonedDateTime]]).
  // 3. Let instant be ! CreateTemporalInstant(zonedDateTime.[[Nanoseconds]]).
  Handle<JSTemporalInstant> instant;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, instant,
      temporal::CreateTemporalInstant(
          isolate, handle(zoned_date_time->nanoseconds(), isolate)),
      String);
  // 4. Return ? BuiltinTimeZoneGetOffsetStringFor(zonedDateTime.[[TimeZone]],
  // instant).
  return BuiltinTimeZoneGetOffsetStringFor(
      isolate, handle(zoned_date_time->time_zone(), isolate), instant,
      "Temporal.ZonedDateTime.prototype.offset");
}

// #sec-temporal.instant
MaybeHandle<JSTemporalInstant> JSTemporalInstant::Constructor(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    Handle<Object> epoch_nanoseconds_obj) {
  TEMPORAL_ENTER_FUNC();
  const char* method_name = "Temporal.Instant";
  // 1. If NewTarget is undefined, then
  if (new_target->IsUndefined()) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     method_name)),
                    JSTemporalInstant);
  }
  // 2. Let epochNanoseconds be ? ToBigInt(epochNanoseconds).
  Handle<BigInt> epoch_nanoseconds;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, epoch_nanoseconds,
                             BigInt::FromObject(isolate, epoch_nanoseconds_obj),
                             JSTemporalInstant);
  // 3. If ! IsValidEpochNanoseconds(epochNanoseconds) is false, throw a
  // RangeError exception.
  if (!IsValidEpochNanoseconds(isolate, epoch_nanoseconds)) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                    JSTemporalInstant);
  }
  // 4. Return ? CreateTemporalInstant(epochNanoseconds, NewTarget).
  return temporal::CreateTemporalInstant(isolate, target, new_target,
                                         epoch_nanoseconds);
}

namespace {

// The logic in Temporal.Instant.fromEpochSeconds and fromEpochMilliseconds,
// are the same except a scaling factor, code all of them into the follow
// function.
MaybeHandle<JSTemporalInstant> ScaleNumberToNanosecondsVerifyAndMake(
    Isolate* isolate, Handle<BigInt> bigint, uint32_t scale) {
  TEMPORAL_ENTER_FUNC();
  DCHECK(scale == 1 || scale == 1000 || scale == 1000000 ||
         scale == 1000000000);
  // 2. Let epochNanoseconds be epochXseconds × scaleℤ.
  Handle<BigInt> epoch_nanoseconds;
  if (scale == 1) {
    epoch_nanoseconds = bigint;
  } else {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, epoch_nanoseconds,
        BigInt::Multiply(isolate, BigInt::FromUint64(isolate, scale), bigint),
        JSTemporalInstant);
  }
  // 3. If ! IsValidEpochNanoseconds(epochNanoseconds) is false, throw a
  // RangeError exception.
  if (!IsValidEpochNanoseconds(isolate, epoch_nanoseconds)) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(),
                    JSTemporalInstant);
  }
  return temporal::CreateTemporalInstant(isolate, epoch_nanoseconds);
}

MaybeHandle<JSTemporalInstant> ScaleNumberToNanosecondsVerifyAndMake(
    Isolate* isolate, Handle<Object> epoch_Xseconds, uint32_t scale) {
  TEMPORAL_ENTER_FUNC();
  // 1. Set epochXseconds to ? ToNumber(epochXseconds).
  ASSIGN_RETURN_ON_EXCEPTION(isolate, epoch_Xseconds,
                             Object::ToNumber(isolate, epoch_Xseconds),
                             JSTemporalInstant);
  // 2. Set epochMilliseconds to ? NumberToBigInt(epochMilliseconds).
  Handle<BigInt> bigint;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, bigint,
                             BigInt::FromNumber(isolate, epoch_Xseconds),
                             JSTemporalInstant);
  return ScaleNumberToNanosecondsVerifyAndMake(isolate, bigint, scale);
}

MaybeHandle<JSTemporalInstant> ScaleToNanosecondsVerifyAndMake(
    Isolate* isolate, Handle<Object> epoch_Xseconds, uint32_t scale) {
  TEMPORAL_ENTER_FUNC();
  // 1. Set epochMicroseconds to ? ToBigInt(epochMicroseconds).
  Handle<BigInt> bigint;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, bigint,
                             BigInt::FromObject(isolate, epoch_Xseconds),
                             JSTemporalInstant);
  return ScaleNumberToNanosecondsVerifyAndMake(isolate, bigint, scale);
}

}  // namespace

// #sec-temporal.instant.fromepochseconds
MaybeHandle<JSTemporalInstant> JSTemporalInstant::FromEpochSeconds(
    Isolate* isolate, Handle<Object> epoch_seconds) {
  TEMPORAL_ENTER_FUNC();
  return ScaleNumberToNanosecondsVerifyAndMake(isolate, epoch_seconds,
                                               1000000000);
}

// #sec-temporal.instant.fromepochmilliseconds
MaybeHandle<JSTemporalInstant> JSTemporalInstant::FromEpochMilliseconds(
    Isolate* isolate, Handle<Object> epoch_milliseconds) {
  TEMPORAL_ENTER_FUNC();
  return ScaleNumberToNanosecondsVerifyAndMake(isolate, epoch_milliseconds,
                                               1000000);
}

// #sec-temporal.instant.fromepochmicroseconds
MaybeHandle<JSTemporalInstant> JSTemporalInstant::FromEpochMicroseconds(
    Isolate* isolate, Handle<Object> epoch_microseconds) {
  TEMPORAL_ENTER_FUNC();
  return ScaleToNanosecondsVerifyAndMake(isolate, epoch_microseconds, 1000);
}

// #sec-temporal.instant.fromepochnanoeconds
MaybeHandle<JSTemporalInstant> JSTemporalInstant::FromEpochNanoseconds(
    Isolate* isolate, Handle<Object> epoch_nanoseconds) {
  TEMPORAL_ENTER_FUNC();
  return ScaleToNanosecondsVerifyAndMake(isolate, epoch_nanoseconds, 1);
}

// #sec-temporal.instant.from
MaybeHandle<JSTemporalInstant> JSTemporalInstant::From(Isolate* isolate,
                                                       Handle<Object> item) {
  TEMPORAL_ENTER_FUNC();
  const char* method_name = "Temporal.Instant.from";
  //  1. If Type(item) is Object and item has an [[InitializedTemporalInstant]]
  //  internal slot, then
  if (item->IsJSTemporalInstant()) {
    // a. Return ? CreateTemporalInstant(item.[[Nanoseconds]]).
    return temporal::CreateTemporalInstant(
        isolate, handle(JSTemporalInstant::cast(*item).nanoseconds(), isolate));
  }
  // 2. Return ? ToTemporalInstant(item).
  return ToTemporalInstant(isolate, item, method_name);
}

namespace temporal {

// Step iii and iv of #sec-temporal.calendar.prototype.fields
MaybeHandle<Oddball> IsInvalidTemporalCalendarField(
    Isolate* isolate, Handle<String> next_value,
    Handle<FixedArray> fields_name) {
  Factory* factory = isolate->factory();
  // iii. iii. If fieldNames contains nextValue, then
  for (int i = 0; i < fields_name->length(); i++) {
    Object item = fields_name->get(i);
    CHECK(item.IsString());
    if (String::Equals(isolate, next_value,
                       handle(String::cast(item), isolate))) {
      return isolate->factory()->true_value();
    }
  }
  // iv. If nextValue is not one of "year", "month", "monthCode", "day", "hour",
  // "minute", "second", "millisecond", "microsecond", "nanosecond", then
  if (!(String::Equals(isolate, next_value, factory->year_string()) ||
        String::Equals(isolate, next_value, factory->month_string()) ||
        String::Equals(isolate, next_value, factory->monthCode_string()) ||
        String::Equals(isolate, next_value, factory->day_string()) ||
        String::Equals(isolate, next_value, factory->hour_string()) ||
        String::Equals(isolate, next_value, factory->minute_string()) ||
        String::Equals(isolate, next_value, factory->second_string()) ||
        String::Equals(isolate, next_value, factory->millisecond_string()) ||
        String::Equals(isolate, next_value, factory->microsecond_string()) ||
        String::Equals(isolate, next_value, factory->nanosecond_string()))) {
    return isolate->factory()->true_value();
  }
  return isolate->factory()->false_value();
}

}  // namespace temporal
}  // namespace internal
}  // namespace v8
