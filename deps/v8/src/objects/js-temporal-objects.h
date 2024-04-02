// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_TEMPORAL_OBJECTS_H_
#define V8_OBJECTS_JS_TEMPORAL_OBJECTS_H_

#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-temporal-objects-tq.inc"

#define DECLARE_TEMPORAL_INLINE_GETTER_SETTER(field) \
  inline void set_##field(int32_t field);            \
  inline int32_t field() const;

#define DECLARE_TEMPORAL_TIME_INLINE_GETTER_SETTER()     \
  DECLARE_TEMPORAL_INLINE_GETTER_SETTER(iso_hour)        \
  DECLARE_TEMPORAL_INLINE_GETTER_SETTER(iso_minute)      \
  DECLARE_TEMPORAL_INLINE_GETTER_SETTER(iso_second)      \
  DECLARE_TEMPORAL_INLINE_GETTER_SETTER(iso_millisecond) \
  DECLARE_TEMPORAL_INLINE_GETTER_SETTER(iso_microsecond) \
  DECLARE_TEMPORAL_INLINE_GETTER_SETTER(iso_nanosecond)

#define DECLARE_TEMPORAL_DATE_INLINE_GETTER_SETTER() \
  DECLARE_TEMPORAL_INLINE_GETTER_SETTER(iso_year)    \
  DECLARE_TEMPORAL_INLINE_GETTER_SETTER(iso_month)   \
  DECLARE_TEMPORAL_INLINE_GETTER_SETTER(iso_day)

#define TEMPORAL_UNIMPLEMENTED(T)            \
  {                                          \
    printf("TBW %s\n", __PRETTY_FUNCTION__); \
    UNIMPLEMENTED();                         \
  }

class JSTemporalPlainDate;
class JSTemporalPlainMonthDay;
class JSTemporalPlainYearMonth;

class JSTemporalCalendar
    : public TorqueGeneratedJSTemporalCalendar<JSTemporalCalendar, JSObject> {
 public:
  // #sec-temporal.calendar
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalCalendar> Constructor(
      Isolate* isolate, Handle<JSFunction> target,
      Handle<HeapObject> new_target, Handle<Object> identifier);

  // #sec-temporal.calendar.prototype.year
  V8_WARN_UNUSED_RESULT static MaybeHandle<Smi> Year(
      Isolate* isolate, Handle<JSTemporalCalendar> calendar,
      Handle<Object> temporal_date_like);

  // #sec-temporal.calendar.prototype.daysinyear
  V8_WARN_UNUSED_RESULT static MaybeHandle<Smi> DaysInYear(
      Isolate* isolate, Handle<JSTemporalCalendar> calendar,
      Handle<Object> temporal_date_like);

  // #sec-temporal.calendar.prototype.dayofweek
  V8_WARN_UNUSED_RESULT static MaybeHandle<Smi> DayOfWeek(
      Isolate* isolate, Handle<JSTemporalCalendar> calendar,
      Handle<Object> temporal_date_like);

  // #sec-temporal.calendar.prototype.dayofyear
  V8_WARN_UNUSED_RESULT static MaybeHandle<Smi> DayOfYear(
      Isolate* isolate, Handle<JSTemporalCalendar> calendar,
      Handle<Object> temporal_date_like);

  // #sec-temporal.calendar.prototype.monthsinyear
  V8_WARN_UNUSED_RESULT static MaybeHandle<Smi> MonthsInYear(
      Isolate* isolate, Handle<JSTemporalCalendar> calendar,
      Handle<Object> temporal_date_like);

  // #sec-temporal.calendar.prototype.inleapyear
  V8_WARN_UNUSED_RESULT static MaybeHandle<Oddball> InLeapYear(
      Isolate* isolate, Handle<JSTemporalCalendar> calendar,
      Handle<Object> temporal_date_like);

  // #sec-temporal.calendar.prototype.daysinmonth
  V8_WARN_UNUSED_RESULT static MaybeHandle<Smi> DaysInMonth(
      Isolate* isolate, Handle<JSTemporalCalendar> calendar,
      Handle<Object> temporal_date_like);

  // #sec-temporal.calendar.prototype.daysinweek
  V8_WARN_UNUSED_RESULT static MaybeHandle<Smi> DaysInWeek(
      Isolate* isolate, Handle<JSTemporalCalendar> calendar,
      Handle<Object> temporal_date_like);

  // #sec-temporal.calendar.prototype.datefromfields
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalPlainDate> DateFromFields(
      Isolate* isolate, Handle<JSTemporalCalendar> calendar,
      Handle<Object> fields, Handle<Object> options);

  // #sec-temporal.calendar.prototype.mergefields
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSReceiver> MergeFields(
      Isolate* isolate, Handle<JSTemporalCalendar> calendar,
      Handle<Object> fields, Handle<Object> additional_fields);

  // #sec-temporal.calendar.prototype.tostring
  static MaybeHandle<String> ToString(Isolate* isolate,
                                      Handle<JSTemporalCalendar> calendar,
                                      const char* method);

  DECL_PRINTER(JSTemporalCalendar)

  DEFINE_TORQUE_GENERATED_JS_TEMPORAL_CALENDAR_FLAGS()

  DECL_INT_ACCESSORS(calendar_index)

  TQ_OBJECT_CONSTRUCTORS(JSTemporalCalendar)
};

class JSTemporalDuration
    : public TorqueGeneratedJSTemporalDuration<JSTemporalDuration, JSObject> {
 public:
  // #sec-temporal.duration
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalDuration> Constructor(
      Isolate* isolate, Handle<JSFunction> target,
      Handle<HeapObject> new_target, Handle<Object> years,
      Handle<Object> months, Handle<Object> weeks, Handle<Object> days,
      Handle<Object> hours, Handle<Object> minutes, Handle<Object> seconds,
      Handle<Object> milliseconds, Handle<Object> microseconds,
      Handle<Object> nanoseconds);

  // #sec-get-temporal.duration.prototype.sign
  V8_WARN_UNUSED_RESULT static MaybeHandle<Smi> Sign(
      Isolate* isolate, Handle<JSTemporalDuration> duration);

  // #sec-get-temporal.duration.prototype.blank
  V8_WARN_UNUSED_RESULT static MaybeHandle<Oddball> Blank(
      Isolate* isolate, Handle<JSTemporalDuration> duration);

  // #sec-temporal.duration.prototype.negated
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalDuration> Negated(
      Isolate* isolate, Handle<JSTemporalDuration> duration);

  // #sec-temporal.duration.prototype.abs
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalDuration> Abs(
      Isolate* isolate, Handle<JSTemporalDuration> duration);

  DECL_PRINTER(JSTemporalDuration)

  TQ_OBJECT_CONSTRUCTORS(JSTemporalDuration)
};

class JSTemporalInstant
    : public TorqueGeneratedJSTemporalInstant<JSTemporalInstant, JSObject> {
 public:
  // #sec-temporal-instant-constructor
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalInstant> Constructor(
      Isolate* isolate, Handle<JSFunction> target,
      Handle<HeapObject> new_target, Handle<Object> epoch_nanoseconds);

  // #sec-temporal.now.instant
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalInstant> Now(
      Isolate* isolate);

  // #sec-temporal.instant.fromepochseconds
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalInstant> FromEpochSeconds(
      Isolate* isolate, Handle<Object> epoch_seconds);
  // #sec-temporal.instant.fromepochmilliseconds
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalInstant>
  FromEpochMilliseconds(Isolate* isolate, Handle<Object> epoch_milliseconds);
  // #sec-temporal.instant.fromepochmicroseconds
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalInstant>
  FromEpochMicroseconds(Isolate* isolate, Handle<Object> epoch_microseconds);
  // #sec-temporal.instant.fromepochnanoeconds
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalInstant>
  FromEpochNanoseconds(Isolate* isolate, Handle<Object> epoch_nanoseconds);

  // #sec-temporal.instant.from
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalInstant> From(
      Isolate* isolate, Handle<Object> item);

  DECL_PRINTER(JSTemporalInstant)

  TQ_OBJECT_CONSTRUCTORS(JSTemporalInstant)
};

class JSTemporalPlainDate
    : public TorqueGeneratedJSTemporalPlainDate<JSTemporalPlainDate, JSObject> {
 public:
  // #sec-temporal-createtemporaldate
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalPlainDate> Constructor(
      Isolate* isolate, Handle<JSFunction> target,
      Handle<HeapObject> new_target, Handle<Object> iso_year,
      Handle<Object> iso_month, Handle<Object> iso_day,
      Handle<Object> calendar_like);

  // #sec-temporal.plaindate.prototype.withcalendar
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalPlainDate> WithCalendar(
      Isolate* isolate, Handle<JSTemporalPlainDate> plain_date,
      Handle<Object> calendar_like);

  // #sec-temporal.plaindate.from
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalPlainDate> From(
      Isolate* isolate, Handle<Object> item, Handle<Object> options);

  // #sec-temporal.plaindate.prototype.getisofields
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSReceiver> GetISOFields(
      Isolate* isolate, Handle<JSTemporalPlainDate> plain_date);

  // #sec-temporal.now.plaindate
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalPlainDate> Now(
      Isolate* isolate, Handle<Object> calendar_like,
      Handle<Object> temporal_time_zone_like);

  // #sec-temporal.now.plaindateiso
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalPlainDate> NowISO(
      Isolate* isolate, Handle<Object> temporal_time_zone_like);
  DECL_PRINTER(JSTemporalPlainDate)

  DEFINE_TORQUE_GENERATED_JS_TEMPORAL_YEAR_MONTH_DAY()

  DECLARE_TEMPORAL_DATE_INLINE_GETTER_SETTER()

  TQ_OBJECT_CONSTRUCTORS(JSTemporalPlainDate)
};

class JSTemporalPlainDateTime
    : public TorqueGeneratedJSTemporalPlainDateTime<JSTemporalPlainDateTime,
                                                    JSObject> {
 public:
  // #sec-temporal-createtemporaldatetime
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalPlainDateTime> Constructor(
      Isolate* isolate, Handle<JSFunction> target,
      Handle<HeapObject> new_target, Handle<Object> iso_year,
      Handle<Object> iso_month, Handle<Object> iso_day, Handle<Object> hour,
      Handle<Object> minute, Handle<Object> second, Handle<Object> millisecond,
      Handle<Object> microsecond, Handle<Object> nanosecond,
      Handle<Object> calendar_like);

  // #sec-temporal.plaindatetime.prototype.withcalendar
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalPlainDateTime>
  WithCalendar(Isolate* isolate, Handle<JSTemporalPlainDateTime> date_time,
               Handle<Object> calendar_like);

  // #sec-temporal.plaindatetime.prototype.getisofields
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSReceiver> GetISOFields(
      Isolate* isolate, Handle<JSTemporalPlainDateTime> date_time);

  // #sec-temporal.now.plaindatetime
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalPlainDateTime> Now(
      Isolate* isolate, Handle<Object> calendar_like,
      Handle<Object> temporal_time_zone_like);

  // #sec-temporal.now.plaindatetimeiso
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalPlainDateTime> NowISO(
      Isolate* isolate, Handle<Object> temporal_time_zone_like);

  DECL_PRINTER(JSTemporalPlainDateTime)

  DEFINE_TORQUE_GENERATED_JS_TEMPORAL_YEAR_MONTH_DAY()
  DEFINE_TORQUE_GENERATED_JS_TEMPORAL_HOUR_MINUTE_SECOND()
  DEFINE_TORQUE_GENERATED_JS_TEMPORAL_SECOND_PARTS()

  DECLARE_TEMPORAL_DATE_INLINE_GETTER_SETTER()
  DECLARE_TEMPORAL_TIME_INLINE_GETTER_SETTER()

  TQ_OBJECT_CONSTRUCTORS(JSTemporalPlainDateTime)
};

class JSTemporalPlainMonthDay
    : public TorqueGeneratedJSTemporalPlainMonthDay<JSTemporalPlainMonthDay,
                                                    JSObject> {
 public:
  // ##sec-temporal.plainmonthday
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalPlainMonthDay> Constructor(
      Isolate* isolate, Handle<JSFunction> target,
      Handle<HeapObject> new_target, Handle<Object> iso_month,
      Handle<Object> iso_day, Handle<Object> calendar_like,
      Handle<Object> reference_iso_year);

  // #sec-temporal.plainmonthday.prototype.getisofields
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSReceiver> GetISOFields(
      Isolate* isolate, Handle<JSTemporalPlainMonthDay> month_day);

  DECL_PRINTER(JSTemporalPlainMonthDay)

  DEFINE_TORQUE_GENERATED_JS_TEMPORAL_YEAR_MONTH_DAY()

  DECLARE_TEMPORAL_DATE_INLINE_GETTER_SETTER()

  TQ_OBJECT_CONSTRUCTORS(JSTemporalPlainMonthDay)
};

class JSTemporalPlainTime
    : public TorqueGeneratedJSTemporalPlainTime<JSTemporalPlainTime, JSObject> {
 public:
  // #sec-temporal-plaintime-constructor
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalPlainTime> Constructor(
      Isolate* isolate, Handle<JSFunction> target,
      Handle<HeapObject> new_target, Handle<Object> hour, Handle<Object> minute,
      Handle<Object> second, Handle<Object> millisecond,
      Handle<Object> microsecond, Handle<Object> nanosecond);

  // #sec-temporal.plaintime.from
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalPlainTime> From(
      Isolate* isolate, Handle<Object> item, Handle<Object> options);

  // #sec-temporal.plaintime.prototype.getisofields
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSReceiver> GetISOFields(
      Isolate* isolate, Handle<JSTemporalPlainTime> plain_time);

  // #sec-temporal.now.plaintimeiso
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalPlainTime> NowISO(
      Isolate* isolate, Handle<Object> temporal_time_zone_like);

  DECL_PRINTER(JSTemporalPlainTime)

  DEFINE_TORQUE_GENERATED_JS_TEMPORAL_HOUR_MINUTE_SECOND()
  DEFINE_TORQUE_GENERATED_JS_TEMPORAL_SECOND_PARTS()

  DECLARE_TEMPORAL_TIME_INLINE_GETTER_SETTER()

  TQ_OBJECT_CONSTRUCTORS(JSTemporalPlainTime)
};

class JSTemporalPlainYearMonth
    : public TorqueGeneratedJSTemporalPlainYearMonth<JSTemporalPlainYearMonth,
                                                     JSObject> {
 public:
  // ##sec-temporal.plainyearmonth
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalPlainYearMonth>
  Constructor(Isolate* isolate, Handle<JSFunction> target,
              Handle<HeapObject> new_target, Handle<Object> iso_year,
              Handle<Object> iso_month, Handle<Object> calendar_like,
              Handle<Object> reference_iso_day);

  // #sec-temporal.plainyearmonth.prototype.getisofields
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSReceiver> GetISOFields(
      Isolate* isolate, Handle<JSTemporalPlainYearMonth> year_month);

  // Abstract Operations

  DECL_PRINTER(JSTemporalPlainYearMonth)

  DEFINE_TORQUE_GENERATED_JS_TEMPORAL_YEAR_MONTH_DAY()

  DECLARE_TEMPORAL_DATE_INLINE_GETTER_SETTER()

  TQ_OBJECT_CONSTRUCTORS(JSTemporalPlainYearMonth)
};

class JSTemporalTimeZone
    : public TorqueGeneratedJSTemporalTimeZone<JSTemporalTimeZone, JSObject> {
 public:
  // #sec-temporal.now.timezone
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalTimeZone> Now(
      Isolate* isolate);

  // #sec-temporal.timezone
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalTimeZone> Constructor(
      Isolate* isolate, Handle<JSFunction> target,
      Handle<HeapObject> new_target, Handle<Object> identifier);

  // #sec-temporal.timezone.prototype.tostring
  static MaybeHandle<Object> ToString(Isolate* isolate,
                                      Handle<JSTemporalTimeZone> time_zone,
                                      const char* method);

  DECL_PRINTER(JSTemporalTimeZone)

  DEFINE_TORQUE_GENERATED_JS_TEMPORAL_TIME_ZONE_FLAGS()
  DEFINE_TORQUE_GENERATED_JS_TEMPORAL_TIME_ZONE_SUB_MILLISECONDS()

  DECL_BOOLEAN_ACCESSORS(is_offset)
  DECL_INT_ACCESSORS(offset_milliseconds_or_time_zone_index)

  DECLARE_TEMPORAL_INLINE_GETTER_SETTER(offset_milliseconds)
  DECLARE_TEMPORAL_INLINE_GETTER_SETTER(offset_sub_milliseconds)

  int32_t time_zone_index() const;
  int64_t offset_nanoseconds() const;
  void set_offset_nanoseconds(int64_t offset_nanoseconds);

  MaybeHandle<String> id(Isolate* isolate) const;

  TQ_OBJECT_CONSTRUCTORS(JSTemporalTimeZone)
};

class JSTemporalZonedDateTime
    : public TorqueGeneratedJSTemporalZonedDateTime<JSTemporalZonedDateTime,
                                                    JSObject> {
 public:
  // #sec-temporal.zoneddatetime
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalZonedDateTime> Constructor(
      Isolate* isolate, Handle<JSFunction> target,
      Handle<HeapObject> new_target, Handle<Object> epoch_nanoseconds,
      Handle<Object> time_zone_like, Handle<Object> calendar_like);

  // #sec-temporal.zoneddatetime.prototype.withcalendar
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalZonedDateTime>
  WithCalendar(Isolate* isolate,
               Handle<JSTemporalZonedDateTime> zoned_date_time,
               Handle<Object> calendar_like);

  // #sec-temporal.zoneddatetime.prototype.withtimezone
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalZonedDateTime>
  WithTimeZone(Isolate* isolate,
               Handle<JSTemporalZonedDateTime> zoned_date_time,
               Handle<Object> time_zone_like);

  // #sec-temporal.zoneddatetime.prototype.getisofields
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSReceiver> GetISOFields(
      Isolate* isolate, Handle<JSTemporalZonedDateTime> zoned_date_time);

  // #sec-temporal.zoneddatetime.prototype.toplainyearmonth
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalPlainYearMonth>
  ToPlainYearMonth(Isolate* isolate,
                   Handle<JSTemporalZonedDateTime> zoned_date_time);

  // #sec-temporal.zoneddatetime.prototype.toplainmonthday
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalPlainMonthDay>
  ToPlainMonthDay(Isolate* isolate,
                  Handle<JSTemporalZonedDateTime> zoned_date_time);

  // #sec-temporal.now.zoneddatetime
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalZonedDateTime> Now(
      Isolate* isolate, Handle<Object> calendar_like,
      Handle<Object> temporal_time_zone_like);

  // #sec-temporal.now.zoneddatetimeiso
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSTemporalZonedDateTime> NowISO(
      Isolate* isolate, Handle<Object> temporal_time_zone_like);

  DECL_PRINTER(JSTemporalZonedDateTime)

  TQ_OBJECT_CONSTRUCTORS(JSTemporalZonedDateTime)
};

namespace temporal {

// #sec-temporal-createtemporalinstant
V8_WARN_UNUSED_RESULT MaybeHandle<JSTemporalInstant> CreateTemporalInstant(
    Isolate* isolate, Handle<JSFunction> target, Handle<HeapObject> new_target,
    Handle<BigInt> epoch_nanoseconds);
V8_WARN_UNUSED_RESULT MaybeHandle<JSTemporalInstant> CreateTemporalInstant(
    Isolate* isolate, Handle<BigInt> epoch_nanoseconds);

// #sec-temporal-calendaryear
#define DECLARE_CALENDAR_ABSTRACT_OPERATION(Name)           \
  V8_WARN_UNUSED_RESULT MaybeHandle<Object> Calendar##Name( \
      Isolate* isolate, Handle<JSReceiver> calendar,        \
      Handle<JSReceiver> date_like);
DECLARE_CALENDAR_ABSTRACT_OPERATION(Year)
DECLARE_CALENDAR_ABSTRACT_OPERATION(Month)
DECLARE_CALENDAR_ABSTRACT_OPERATION(MonthCode)
DECLARE_CALENDAR_ABSTRACT_OPERATION(Day)

#ifdef V8_INTL_SUPPORT
DECLARE_CALENDAR_ABSTRACT_OPERATION(Era)
DECLARE_CALENDAR_ABSTRACT_OPERATION(EraYear)
#endif  //  V8_INTL_SUPPORT

#undef DECLARE_CALENDAR_ABSTRACT_OPERATION

// #sec-temporal-getiso8601calendar
V8_WARN_UNUSED_RESULT MaybeHandle<JSTemporalCalendar> GetISO8601Calendar(
    Isolate* isolate);

// #sec-temporal-builtintimezonegetplaindatetimefor
V8_WARN_UNUSED_RESULT MaybeHandle<JSTemporalPlainDateTime>
BuiltinTimeZoneGetPlainDateTimeFor(Isolate* isolate,
                                   Handle<JSReceiver> time_zone,
                                   Handle<JSTemporalInstant> instant,
                                   Handle<JSReceiver> calendar,
                                   const char* method);

V8_WARN_UNUSED_RESULT MaybeHandle<Object> InvokeCalendarMethod(
    Isolate* isolate, Handle<JSReceiver> calendar, Handle<String> name,
    Handle<JSReceiver> temporal_like);

V8_WARN_UNUSED_RESULT MaybeHandle<JSReceiver> ToTemporalCalendar(
    Isolate* isolate, Handle<Object> temporal_calendar_like,
    const char* method);

V8_WARN_UNUSED_RESULT MaybeHandle<JSReceiver> ToTemporalTimeZone(
    Isolate* isolate, Handle<Object> temporal_time_zone_like,
    const char* method);

}  // namespace temporal
}  // namespace internal
}  // namespace v8
#include "src/objects/object-macros-undef.h"
#endif  // V8_OBJECTS_JS_TEMPORAL_OBJECTS_H_
