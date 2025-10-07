// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_TEMPORAL_OBJECTS_H_
#define V8_OBJECTS_JS_TEMPORAL_OBJECTS_H_

#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/js-temporal-helpers.h"
#include "src/objects/managed.h"
#include "src/objects/objects.h"
#include "temporal_rs/Instant.d.hpp"
#include "temporal_rs/PlainDate.d.hpp"
#include "temporal_rs/PlainDateTime.d.hpp"
#include "temporal_rs/PlainMonthDay.d.hpp"
#include "temporal_rs/PlainTime.d.hpp"
#include "temporal_rs/PlainYearMonth.d.hpp"
#include "temporal_rs/TimeZone.d.hpp"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-temporal-objects-tq.inc"

// For a type wrapping a rust field, add accessors for it
// including a initialize_with_wrapped_rust_value() that can be used in
// templates
#define DECL_ACCESSORS_FOR_RUST_WRAPPER(field, RustType_) \
  typedef RustType_ RustType;                             \
  DECL_ACCESSORS(field, Tagged<Managed<RustType_>>)       \
  inline void initialize_with_wrapped_rust_value(         \
      Tagged<Managed<RustType_>> handle);                 \
  inline const RustType_& wrapped_rust() const;

// Adds JSTemporalFoo::GetConstructorTarget()
// that can be used to obtain a constructor target/new_target for constructing
// values of this type.
#define DECL_CTOR_HELPER() \
  static inline DirectHandle<JSFunction> GetConstructorTarget(Isolate* isolate);

// When populating this list, consider also adding the field to
// js-temporal-objects.tq, adding DEFINE_ACCESSORS_FOR_RUST_WRAPPER
// to js-temporal-objects.cc, and adding an ACCESSORS entry to
// js-temporal-objects-inl.h
ASSIGN_EXTERNAL_POINTER_TAG_FOR_MANAGED(temporal_rs::Instant,
                                        kTemporalInstantTag)
ASSIGN_EXTERNAL_POINTER_TAG_FOR_MANAGED(temporal_rs::Duration,
                                        kTemporalDurationTag)
ASSIGN_EXTERNAL_POINTER_TAG_FOR_MANAGED(temporal_rs::PlainDate,
                                        kTemporalPlainDateTag)
ASSIGN_EXTERNAL_POINTER_TAG_FOR_MANAGED(temporal_rs::PlainDateTime,
                                        kTemporalPlainDateTimeTag)
ASSIGN_EXTERNAL_POINTER_TAG_FOR_MANAGED(temporal_rs::PlainMonthDay,
                                        kTemporalPlainMonthDayTag)
ASSIGN_EXTERNAL_POINTER_TAG_FOR_MANAGED(temporal_rs::PlainTime,
                                        kTemporalPlainTimeTag)
ASSIGN_EXTERNAL_POINTER_TAG_FOR_MANAGED(temporal_rs::PlainYearMonth,
                                        kTemporalPlainYearMonthTag)
ASSIGN_EXTERNAL_POINTER_TAG_FOR_MANAGED(temporal_rs::ZonedDateTime,
                                        kTemporalZonedDateTimeTag)
class JSTemporalPlainDate;
class JSTemporalPlainMonthDay;
class JSTemporalPlainYearMonth;

class JSTemporalDuration
    : public TorqueGeneratedJSTemporalDuration<JSTemporalDuration, JSObject> {
 public:
  // #sec-temporal.duration
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration>
  Constructor(Isolate* isolate, DirectHandle<JSFunction> target,
              DirectHandle<HeapObject> new_target, DirectHandle<Object> years,
              DirectHandle<Object> months, DirectHandle<Object> weeks,
              DirectHandle<Object> days, DirectHandle<Object> hours,
              DirectHandle<Object> minutes, DirectHandle<Object> seconds,
              DirectHandle<Object> milliseconds,
              DirectHandle<Object> microseconds,
              DirectHandle<Object> nanoseconds);

  // #sec-temporal.duration.compare
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Smi> Compare(
      Isolate* isolate, DirectHandle<Object> one, DirectHandle<Object> two,
      DirectHandle<Object> options);

  // #sec-temporal.duration.from
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> From(
      Isolate* isolate, DirectHandle<Object> item);

  // #sec-get-temporal.duration.prototype.sign
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Smi> Sign(
      Isolate* isolate, DirectHandle<JSTemporalDuration> duration);

  // #sec-get-temporal.duration.prototype.blank
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Oddball> Blank(
      Isolate* isolate, DirectHandle<JSTemporalDuration> duration);

  // #sec-temporal.duration.prototype.negated
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> Negated(
      Isolate* isolate, DirectHandle<JSTemporalDuration> duration);

  // #sec-temporal.duration.prototype.abs
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> Abs(
      Isolate* isolate, DirectHandle<JSTemporalDuration> duration);

  // #sec-temporal.duration.prototype.add
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> Add(
      Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
      DirectHandle<Object> other, DirectHandle<Object> options);

  // #sec-temporal.duration.prototype.subtract
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> Subtract(
      Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
      DirectHandle<Object> other, DirectHandle<Object> options);

  // #sec-temporal.duration.prototype.round
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> Round(
      Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
      DirectHandle<Object> round_to_obj);

  // #sec-temporal.duration.prototype.total
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Number> Total(
      Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
      DirectHandle<Object> total_of);

  // #sec-temporal.duration.prototype.tojson
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToJSON(
      Isolate* isolate, DirectHandle<JSTemporalDuration> duration);

  // #sec-temporal.duration.prototype.tolocalestring
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToLocaleString(
      Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
      DirectHandle<Object> locales, DirectHandle<Object> options);

  // #sec-temporal.duration.prototype.tostring
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToString(
      Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
      DirectHandle<Object> options);

  // #sec-temporal.duration.prototype.with
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> With(
      Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
      DirectHandle<Object> temporal_duration_like);

  DECL_PRINTER(JSTemporalDuration)

  DECL_CTOR_HELPER()
  static constexpr bool kTypeContainsCalendar = false;
  DECL_ACCESSORS_FOR_RUST_WRAPPER(duration, temporal_rs::Duration)

  TQ_OBJECT_CONSTRUCTORS(JSTemporalDuration)
};

class JSTemporalInstant
    : public TorqueGeneratedJSTemporalInstant<JSTemporalInstant, JSObject> {
 public:
  // #sec-temporal-instant-constructor
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalInstant> Constructor(
      Isolate* isolate, DirectHandle<JSFunction> target,
      DirectHandle<HeapObject> new_target,
      DirectHandle<Object> epoch_nanoseconds);

  // #sec-temporal.now.instant
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalInstant> Now(
      Isolate* isolate);

  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalInstant> From(
      Isolate* isolate, DirectHandle<Object> item);
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalInstant>
  FromEpochMilliseconds(Isolate* isolate, DirectHandle<Object> item);
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalInstant>
  FromEpochNanoseconds(Isolate* isolate, DirectHandle<Object> item);

  // #sec-temporal.instant.compare
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Smi> Compare(
      Isolate* isolate, DirectHandle<Object> one, DirectHandle<Object> two);

  // #sec-temporal.instant.prototype.round
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalInstant> Round(
      Isolate* isolate, DirectHandle<JSTemporalInstant> instant,
      DirectHandle<Object> round_to);

  // #sec-temporal.instant.prototype.epochmilliseconds
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Number> EpochMilliseconds(
      Isolate* isolate, DirectHandle<JSTemporalInstant> instant);

  // #sec-temporal.instant.prototype.epochnanoseconds
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<BigInt> EpochNanoseconds(
      Isolate* isolate, DirectHandle<JSTemporalInstant> instant);

  // #sec-temporal.instant.prototype.tozoneddatetimeiso
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalZonedDateTime>
  ToZonedDateTimeISO(Isolate* isolate, DirectHandle<JSTemporalInstant> instant,
                     DirectHandle<Object> item);

  // #sec-temporal.instant.prototype.equals
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Oddball> Equals(
      Isolate* isolate, DirectHandle<JSTemporalInstant> instant,
      DirectHandle<Object> other);

  // #sec-temporal.instant.prototype.add
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalInstant> Add(
      Isolate* isolate, DirectHandle<JSTemporalInstant> instant,
      DirectHandle<Object> temporal_duration_like);

  // #sec-temporal.instant.prototype.subtract
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalInstant> Subtract(
      Isolate* isolate, DirectHandle<JSTemporalInstant> instant,
      DirectHandle<Object> temporal_duration_like);

  // #sec-temporal.instant.prototype.tojson
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToJSON(
      Isolate* isolate, DirectHandle<JSTemporalInstant> instant);

  // #sec-temporal.instant.prototype.tolocalestring
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToLocaleString(
      Isolate* isolate, DirectHandle<JSTemporalInstant> instant,
      DirectHandle<Object> locales, DirectHandle<Object> options);

  // #sec-temporal.instant.prototype.tostring
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToString(
      Isolate* isolate, DirectHandle<JSTemporalInstant> instant,
      DirectHandle<Object> options);

  // #sec-temporal.instant.prototype.until
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> Until(
      Isolate* isolate, DirectHandle<JSTemporalInstant> instant,
      DirectHandle<Object> other, DirectHandle<Object> options);

  // #sec-temporal.instant.prototype.since
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> Since(
      Isolate* isolate, DirectHandle<JSTemporalInstant> instant,
      DirectHandle<Object> other, DirectHandle<Object> options);

  DECL_CTOR_HELPER()
  static constexpr bool kTypeContainsCalendar = false;
  DECL_ACCESSORS_FOR_RUST_WRAPPER(instant, temporal_rs::Instant)

  DECL_PRINTER(JSTemporalInstant)

  TQ_OBJECT_CONSTRUCTORS(JSTemporalInstant)
};

class JSTemporalPlainDate
    : public TorqueGeneratedJSTemporalPlainDate<JSTemporalPlainDate, JSObject> {
 public:
  // #sec-temporal-createtemporaldate
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDate>
  Constructor(Isolate* isolate, DirectHandle<JSFunction> target,
              DirectHandle<HeapObject> new_target,
              DirectHandle<Object> iso_year, DirectHandle<Object> iso_month,
              DirectHandle<Object> iso_day, DirectHandle<Object> calendar_like);

  // #sec-temporal.plaindate.compare
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Smi> Compare(
      Isolate* isolate, DirectHandle<Object> one, DirectHandle<Object> two);

  // #sec-temporal.plaindate.prototype.equals
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Oddball> Equals(
      Isolate* isolate, DirectHandle<JSTemporalPlainDate> plain_date,
      DirectHandle<Object> other);


  // #sec-temporal.plaindate.prototype.toplaindatetime
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDateTime>
  ToPlainDateTime(Isolate* isolate,
                  DirectHandle<JSTemporalPlainDate> plain_date,
                  DirectHandle<Object> temporal_time);

  // #sec-temporal.plaindate.prototype.with
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDate> With(
      Isolate* isolate, DirectHandle<JSTemporalPlainDate> plain_date,
      DirectHandle<Object> temporal_duration_like,
      DirectHandle<Object> options);

  // #sec-temporal.plaindate.prototype.withcalendar
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDate>
  WithCalendar(Isolate* isolate, DirectHandle<JSTemporalPlainDate> plain_date,
               DirectHandle<Object> calendar_id);

  // #sec-temporal.plaindate.from
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDate> From(
      Isolate* isolate, DirectHandle<Object> item,
      DirectHandle<Object> options);

  // #sec-temporal.plaindate.prototype.add
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDate> Add(
      Isolate* isolate, DirectHandle<JSTemporalPlainDate> plain_date,
      DirectHandle<Object> temporal_duration_like,
      DirectHandle<Object> options);

  // #sec-temporal.plaindate.prototype.subtract
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDate> Subtract(
      Isolate* isolate, DirectHandle<JSTemporalPlainDate> plain_date,
      DirectHandle<Object> temporal_duration_like,
      DirectHandle<Object> options);

  // #sec-temporal.plaindate.prototype.until
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> Until(
      Isolate* isolate, DirectHandle<JSTemporalPlainDate> plain_date,
      DirectHandle<Object> other, DirectHandle<Object> options);

  // #sec-temporal.plaindate.prototype.since
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> Since(
      Isolate* isolate, DirectHandle<JSTemporalPlainDate> plain_date,
      DirectHandle<Object> other, DirectHandle<Object> options);

  // #sec-temporal.plaindate.prototype.toplainyearmonth
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainYearMonth>
  ToPlainYearMonth(Isolate* isolate,
                   DirectHandle<JSTemporalPlainDate> plain_date);

  // #sec-temporal.plaindate.prototype.toplainmonthday
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainMonthDay>
  ToPlainMonthDay(Isolate* isolate,
                  DirectHandle<JSTemporalPlainDate> plain_date);

  // #sec-temporal.plaindate.prototype.tozoneddatetime
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalZonedDateTime>
  ToZonedDateTime(Isolate* isolate,
                  DirectHandle<JSTemporalPlainDate> plain_date,
                  DirectHandle<Object> item);

  // #sec-temporal.now.plaindateiso
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDate> NowISO(
      Isolate* isolate, DirectHandle<Object> temporal_time_zone_like);

  // #sec-temporal.plaindate.prototype.tostring
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToString(
      Isolate* isolate, DirectHandle<JSTemporalPlainDate> plain_date,
      DirectHandle<Object> options);

  // #sec-temporal.plaindate.prototype.tojson
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToJSON(
      Isolate* isolate, DirectHandle<JSTemporalPlainDate> plain_date);

  // #sec-temporal.plaindate.prototype.tolocalestring
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToLocaleString(
      Isolate* isolate, DirectHandle<JSTemporalPlainDate> plain_date,
      DirectHandle<Object> locales, DirectHandle<Object> options);

  // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporaldate
  V8_WARN_UNUSED_RESULT Maybe<int64_t> GetEpochMillisecondsFor(
      Isolate* isolate, std::string_view time_zone);

  DECL_PRINTER(JSTemporalPlainDate)
  DECL_CTOR_HELPER()
  static constexpr bool kTypeContainsCalendar = true;
  DECL_ACCESSORS_FOR_RUST_WRAPPER(date, temporal_rs::PlainDate)

  TQ_OBJECT_CONSTRUCTORS(JSTemporalPlainDate)
};

class JSTemporalPlainDateTime
    : public TorqueGeneratedJSTemporalPlainDateTime<JSTemporalPlainDateTime,
                                                    JSObject> {
 public:
  // #sec-temporal-createtemporaldatetime
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDateTime>
  Constructor(Isolate* isolate, DirectHandle<JSFunction> target,
              DirectHandle<HeapObject> new_target,
              DirectHandle<Object> iso_year, DirectHandle<Object> iso_month,
              DirectHandle<Object> iso_day, DirectHandle<Object> hour,
              DirectHandle<Object> minute, DirectHandle<Object> second,
              DirectHandle<Object> millisecond,
              DirectHandle<Object> microsecond, DirectHandle<Object> nanosecond,
              DirectHandle<Object> calendar_like);

  // #sec-temporal.plaindatetime.prototype.withplaintime
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDateTime>
  WithPlainTime(Isolate* isolate,
                DirectHandle<JSTemporalPlainDateTime> date_time,
                DirectHandle<Object> temporal_time_like);


  // #sec-temporal.plaindatetime.from
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDateTime> From(
      Isolate* isolate, DirectHandle<Object> item,
      DirectHandle<Object> options);

  // #sec-temporal.plaindatetime.compare
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Smi> Compare(
      Isolate* isolate, DirectHandle<Object> one, DirectHandle<Object> two);

  // #sec-temporal.plaindatetime.prototype.equals
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Oddball> Equals(
      Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> plain_date,
      DirectHandle<Object> other);

  // #sec-temporal.plaintime.prototype.tozoneddatetime
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalZonedDateTime>
  ToZonedDateTime(Isolate* isolate,
                  DirectHandle<JSTemporalPlainDateTime> date_time,
                  DirectHandle<Object> temporal_time_zone_like,
                  DirectHandle<Object> options_obj);

  // #sec-temporal.plaindatetime.prototype.with
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDateTime> With(
      Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
      DirectHandle<Object> temporal_date_time_like,
      DirectHandle<Object> options);
  // #sec-temporal.plaindatetime.prototype.withcalendar
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDateTime>
  WithCalendar(Isolate* isolate,
               DirectHandle<JSTemporalPlainDateTime> plain_date,
               DirectHandle<Object> calendar_id);

  // #sec-temporal.plaindatetime.prototype.tojson
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToJSON(
      Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time);

  // #sec-temporal.plaindatetime.prototype.tolocalestring
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToLocaleString(
      Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
      DirectHandle<Object> locales, DirectHandle<Object> options);

  // #sec-temporal.plaindatetime.prototype.tostring
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToString(
      Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
      DirectHandle<Object> options);

  // #sec-temporal.plaindatetime.prototype.round
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDateTime> Round(
      Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
      DirectHandle<Object> round_to);

  // #sec-temporal.plaindatetime.prototype.until
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> Until(
      Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
      DirectHandle<Object> other, DirectHandle<Object> options);

  // #sec-temporal.plaindatetime.prototype.since
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> Since(
      Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
      DirectHandle<Object> other, DirectHandle<Object> options);

  // #sec-temporal.plaindatetime.prototype.add
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDateTime> Add(
      Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
      DirectHandle<Object> temporal_duration_like,
      DirectHandle<Object> options);

  // #sec-temporal.plaindatetime.prototype.subtract
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDateTime>
  Subtract(Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
           DirectHandle<Object> temporal_duration_like,
           DirectHandle<Object> options);

  // #sec-temporal.now.plaindatetimeiso
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDateTime>
  NowISO(Isolate* isolate, DirectHandle<Object> temporal_time_zone_like);

  // #sec-temporal.plaindatetime.prototype.toplaindate
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDate>
  ToPlainDate(Isolate* isolate,
              DirectHandle<JSTemporalPlainDateTime> date_time);

  // #sec-temporal.plaindatetime.prototype.toplaintime
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainTime>
  ToPlainTime(Isolate* isolate,
              DirectHandle<JSTemporalPlainDateTime> date_time);

  // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporaldate
  V8_WARN_UNUSED_RESULT Maybe<int64_t> GetEpochMillisecondsFor(
      Isolate* isolate, std::string_view time_zone);

  DECL_PRINTER(JSTemporalPlainDateTime)

  DECL_CTOR_HELPER()
  static constexpr bool kTypeContainsCalendar = true;
  DECL_ACCESSORS_FOR_RUST_WRAPPER(date_time, temporal_rs::PlainDateTime)
  TQ_OBJECT_CONSTRUCTORS(JSTemporalPlainDateTime)
};

class JSTemporalPlainMonthDay
    : public TorqueGeneratedJSTemporalPlainMonthDay<JSTemporalPlainMonthDay,
                                                    JSObject> {
 public:
  // ##sec-temporal.plainmonthday
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainMonthDay>
  Constructor(Isolate* isolate, DirectHandle<JSFunction> target,
              DirectHandle<HeapObject> new_target,
              DirectHandle<Object> iso_month, DirectHandle<Object> iso_day,
              DirectHandle<Object> calendar_like,
              DirectHandle<Object> reference_iso_year);

  // #sec-temporal.plainmonthday.from
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainMonthDay> From(
      Isolate* isolate, DirectHandle<Object> item,
      DirectHandle<Object> options);

  // #sec-temporal.plainmonthday.prototype.equals
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Oddball> Equals(
      Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> month_day,
      DirectHandle<Object> other);

  // #sec-temporal.plainmonthday.prototype.with
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainMonthDay> With(
      Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> month_day,
      DirectHandle<Object> temporal_month_day_like,
      DirectHandle<Object> options);

  // #sec-temporal.plainmonthday.prototype.toplaindate
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDate>
  ToPlainDate(Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> month_day,
              DirectHandle<Object> item);

  // #sec-temporal.plainmonthday.prototype.getisofields
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSReceiver> GetISOFields(
      Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> month_day);

  // #sec-temporal.plainmonthday.prototype.tostring
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToString(
      Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> month_day,
      DirectHandle<Object> options);

  // #sec-temporal.plainmonthday.prototype.tojson
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToJSON(
      Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> month_day);

  // #sec-temporal.plainmonthday.prototype.tolocalestring
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToLocaleString(
      Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> plain_date,
      DirectHandle<Object> locales, DirectHandle<Object> options);

  // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporalmonthday
  V8_WARN_UNUSED_RESULT Maybe<int64_t> GetEpochMillisecondsFor(
      Isolate* isolate, std::string_view time_zone);

  DECL_PRINTER(JSTemporalPlainMonthDay)

  DECL_CTOR_HELPER()
  static constexpr bool kTypeContainsCalendar = true;
  DECL_ACCESSORS_FOR_RUST_WRAPPER(month_day, temporal_rs::PlainMonthDay)

  TQ_OBJECT_CONSTRUCTORS(JSTemporalPlainMonthDay)
};

class JSTemporalPlainTime
    : public TorqueGeneratedJSTemporalPlainTime<JSTemporalPlainTime, JSObject> {
 public:
  // #sec-temporal-plaintime-constructor
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainTime>
  Constructor(Isolate* isolate, DirectHandle<JSFunction> target,
              DirectHandle<HeapObject> new_target, DirectHandle<Object> hour,
              DirectHandle<Object> minute, DirectHandle<Object> second,
              DirectHandle<Object> millisecond,
              DirectHandle<Object> microsecond,
              DirectHandle<Object> nanosecond);

  // #sec-temporal.plaintime.compare
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Smi> Compare(
      Isolate* isolate, DirectHandle<Object> one, DirectHandle<Object> two);

  // #sec-temporal.plaintime.prototype.equals
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Oddball> Equals(
      Isolate* isolate, DirectHandle<JSTemporalPlainTime> plain_date,
      DirectHandle<Object> other);

  // #sec-temporal.plaintime.from
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainTime> From(
      Isolate* isolate, DirectHandle<Object> item,
      DirectHandle<Object> options);

  // #sec-temporal.plaintime.prototype.tozoneddatetime
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalZonedDateTime>
  ToZonedDateTime(Isolate* isolate,
                  DirectHandle<JSTemporalPlainTime> plain_time,
                  DirectHandle<Object> item);

  // #sec-temporal.plaintime.prototype.add
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainTime> Add(
      Isolate* isolate, DirectHandle<JSTemporalPlainTime> plain_time,
      DirectHandle<Object> temporal_duration_like);

  // #sec-temporal.plaintime.prototype.subtract
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainTime> Subtract(
      Isolate* isolate, DirectHandle<JSTemporalPlainTime> plain_time,
      DirectHandle<Object> temporal_duration_like);

  // #sec-temporal.plaintime.prototype.until
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> Until(
      Isolate* isolate, DirectHandle<JSTemporalPlainTime> plain_time,
      DirectHandle<Object> other, DirectHandle<Object> options);

  // #sec-temporal.plaintime.prototype.since
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> Since(
      Isolate* isolate, DirectHandle<JSTemporalPlainTime> plain_time,
      DirectHandle<Object> other, DirectHandle<Object> options);

  // #sec-temporal.plaintime.prototype.round
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainTime> Round(
      Isolate* isolate, DirectHandle<JSTemporalPlainTime> plain_time,
      DirectHandle<Object> round_to);

  // #sec-temporal.plaintime.prototype.with
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainTime> With(
      Isolate* isolate, DirectHandle<JSTemporalPlainTime> plain_time,
      DirectHandle<Object> temporal_time_like, DirectHandle<Object> options);

  // #sec-temporal.now.plaintimeiso
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainTime> NowISO(
      Isolate* isolate, DirectHandle<Object> temporal_time_zone_like);

  // #sec-temporal.plaintime.prototype.tojson
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToJSON(
      Isolate* isolate, DirectHandle<JSTemporalPlainTime> plain_time);

  // #sec-temporal.plaintime.prototype.tostring
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToString(
      Isolate* isolate, DirectHandle<JSTemporalPlainTime> plain_time,
      DirectHandle<Object> options);

  // #sec-temporal.plaintime.prototype.tolocalestring
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToLocaleString(
      Isolate* isolate, DirectHandle<JSTemporalPlainTime> plain_time,
      DirectHandle<Object> locales, DirectHandle<Object> options);

  // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporaldate
  V8_WARN_UNUSED_RESULT Maybe<int64_t> GetEpochMillisecondsFor(
      Isolate* isolate, std::string_view time_zone);

  DECL_PRINTER(JSTemporalPlainTime)

  DECL_CTOR_HELPER()
  static constexpr bool kTypeContainsCalendar = false;
  DECL_ACCESSORS_FOR_RUST_WRAPPER(time, temporal_rs::PlainTime)
  TQ_OBJECT_CONSTRUCTORS(JSTemporalPlainTime)
};

class JSTemporalPlainYearMonth
    : public TorqueGeneratedJSTemporalPlainYearMonth<JSTemporalPlainYearMonth,
                                                     JSObject> {
 public:
  // ##sec-temporal.plainyearmonth
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainYearMonth>
  Constructor(Isolate* isolate, DirectHandle<JSFunction> target,
              DirectHandle<HeapObject> new_target,
              DirectHandle<Object> iso_year, DirectHandle<Object> iso_month,
              DirectHandle<Object> calendar_like,
              DirectHandle<Object> reference_iso_day);

  // #sec-temporal.plainyearmonth.from
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainYearMonth> From(
      Isolate* isolate, DirectHandle<Object> item,
      DirectHandle<Object> options);

  // #sec-temporal.plainyearmonth.compare
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Smi> Compare(
      Isolate* isolate, DirectHandle<Object> one, DirectHandle<Object> two);

  // #sec-temporal.plainyearmonth.prototype.equals
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Oddball> Equals(
      Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
      DirectHandle<Object> other);

  // #sec-temporal.plainyearmonth.prototype.with
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainYearMonth> With(
      Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
      DirectHandle<Object> temporal_year_month_like,
      DirectHandle<Object> options);

  // #sec-temporal.plainyearmonth.prototype.toplaindate
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDate>
  ToPlainDate(Isolate* isolate,
              DirectHandle<JSTemporalPlainYearMonth> year_month,
              DirectHandle<Object> item);

  // #sec-temporal.plainyearmonth.prototype.add
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainYearMonth> Add(
      Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
      DirectHandle<Object> temporal_duration_like,
      DirectHandle<Object> options);

  // #sec-temporal.plainyearmonth.prototype.subtract
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainYearMonth>
  Subtract(Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
           DirectHandle<Object> temporal_duration_like,
           DirectHandle<Object> options);

  // #sec-temporal.plainyearmonth.prototype.until
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> Until(
      Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
      DirectHandle<Object> other, DirectHandle<Object> options);

  // #sec-temporal.plaindyearmonth.prototype.since
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> Since(
      Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
      DirectHandle<Object> other, DirectHandle<Object> options);

  // #sec-temporal.plainyearmonth.prototype.tostring
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToString(
      Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
      DirectHandle<Object> options);

  // #sec-temporal.plainyearmonth.prototype.tojson
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToJSON(
      Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month);

  // #sec-temporal.plainyearmonth.prototype.tolocalestring
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToLocaleString(
      Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> plain_date,
      DirectHandle<Object> locales, DirectHandle<Object> options);

  // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporaldate
  V8_WARN_UNUSED_RESULT Maybe<int64_t> GetEpochMillisecondsFor(
      Isolate* isolate, std::string_view time_zone);

  // Abstract Operations

  DECL_PRINTER(JSTemporalPlainYearMonth)

  DECL_CTOR_HELPER()
  static constexpr bool kTypeContainsCalendar = true;
  DECL_ACCESSORS_FOR_RUST_WRAPPER(year_month, temporal_rs::PlainYearMonth)
  TQ_OBJECT_CONSTRUCTORS(JSTemporalPlainYearMonth)
};

class JSTemporalZonedDateTime
    : public TorqueGeneratedJSTemporalZonedDateTime<JSTemporalZonedDateTime,
                                                    JSObject> {
 public:
  // #sec-temporal.zoneddatetime
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalZonedDateTime>
  Constructor(Isolate* isolate, DirectHandle<JSFunction> target,
              DirectHandle<HeapObject> new_target,
              DirectHandle<Object> epoch_nanoseconds,
              DirectHandle<Object> time_zone_like,
              DirectHandle<Object> calendar_like);

  // #sec-temporal.zoneddatetime.from
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalZonedDateTime> From(
      Isolate* isolate, DirectHandle<Object> item,
      DirectHandle<Object> options);

  // #sec-temporal.zoneddatetime.compare
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Smi> Compare(
      Isolate* isolate, DirectHandle<Object> one, DirectHandle<Object> two);

  // #sec-temporal.zoneddatetime.prototype.equals
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Oddball> Equals(
      Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
      DirectHandle<Object> other);

  // #sec-temporal.zoneddatetime.prototype.with
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalZonedDateTime> With(
      Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
      DirectHandle<Object> temporal_zoned_date_time_like,
      DirectHandle<Object> options);
  // #sec-temporal.zoneddatetime.prototype.withcalendar
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalZonedDateTime>
  WithCalendar(Isolate* isolate,
               DirectHandle<JSTemporalZonedDateTime> plain_date,
               DirectHandle<Object> calendar_id);

  // #sec-temporal.zoneddatetime.prototype.withplaintime
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalZonedDateTime>
  WithPlainTime(Isolate* isolate,
                DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
                DirectHandle<Object> plain_time_like);

  // #sec-temporal.zoneddatetime.prototype.withtimezone
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalZonedDateTime>
  WithTimeZone(Isolate* isolate,
               DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
               DirectHandle<Object> time_zone_like);

  // #sec-get-temporal.zoneddatetime.prototype.hoursinday
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Number> HoursInDay(
      Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time);

  // #sec-temporal.zoneddatetime.prototype.round
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalZonedDateTime> Round(
      Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
      DirectHandle<Object> round_to);

  // #sec-temporal.zoneddatetime.prototype.until
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> Until(
      Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> date_time,
      DirectHandle<Object> other, DirectHandle<Object> options);

  // #sec-temporal.zoneddatetime.prototype.since
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalDuration> Since(
      Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> date_time,
      DirectHandle<Object> other, DirectHandle<Object> options);

  // #sec-temporal.zoneddatetime.prototype.add
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalZonedDateTime> Add(
      Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
      DirectHandle<Object> temporal_duration_like,
      DirectHandle<Object> options);

  // #sec-temporal.zoneddatetime.prototype.subtract
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalZonedDateTime>
  Subtract(Isolate* isolate,
           DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
           DirectHandle<Object> temporal_duration_like,
           DirectHandle<Object> options);

  // #sec-temporal.now.zoneddatetimeiso
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalZonedDateTime>
  NowISO(Isolate* isolate, DirectHandle<Object> temporal_time_zone_like);

  // #sec-temporal.zoneddatetime.prototype.epochnanoseconds
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<BigInt> EpochNanoseconds(
      Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> instant);

  // #sec-temporal.zoneddatetime.prototype.timezoneid
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> TimeZoneId(
      Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time);

  // #sec-temporal.zoneddatetime.prototype.offsetnanoseconds
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Object> OffsetNanoseconds(
      Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time);

  // #sec-temporal.zoneddatetime.prototype.offset
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> Offset(
      Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time);

  // #sec-temporal.zoneddatetime.prototype.startofday
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalZonedDateTime>
  StartOfDay(Isolate* isolate,
             DirectHandle<JSTemporalZonedDateTime> zoned_date_time);

  // #sec-temporal.zoneddatetime.prototype.gettimezonetransition
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<
      UnionOf<JSTemporalZonedDateTime, Null>>
  GetTimeZoneTransition(Isolate* isolate,
                        DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
                        DirectHandle<Object> direction_param);

  // #sec-temporal.zoneddatetime.prototype.toinstant
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalInstant> ToInstant(
      Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time);

  // #sec-temporal.zoneddatetime.prototype.toplaindate
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDate>
  ToPlainDate(Isolate* isolate,
              DirectHandle<JSTemporalZonedDateTime> zoned_date_time);

  // #sec-temporal.zoneddatetime.prototype.toplaintime
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainTime>
  ToPlainTime(Isolate* isolate,
              DirectHandle<JSTemporalZonedDateTime> zoned_date_time);

  // #sec-temporal.zoneddatetime.prototype.toplaindatetime
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSTemporalPlainDateTime>
  ToPlainDateTime(Isolate* isolate,
                  DirectHandle<JSTemporalZonedDateTime> zoned_date_time);

  // #sec-temporal.zoneddatetime.prototype.tojson
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToJSON(
      Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time);

  // #sec-temporal.zoneddatetime.prototype.tolocalestring
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToLocaleString(
      Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
      DirectHandle<Object> locales, DirectHandle<Object> options);

  // #sec-temporal.zoneddatetime.prototype.tostring
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ToString(
      Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
      DirectHandle<Object> options);

  DECL_PRINTER(JSTemporalZonedDateTime)

  DECL_CTOR_HELPER()
  static constexpr bool kTypeContainsCalendar = true;
  DECL_ACCESSORS_FOR_RUST_WRAPPER(zoned_date_time, temporal_rs::ZonedDateTime)

  TQ_OBJECT_CONSTRUCTORS(JSTemporalZonedDateTime)
};

// #sec-temporal.now.timezoneid
V8_WARN_UNUSED_RESULT MaybeDirectHandle<String> JSTemporalNowTimeZoneId(
    Isolate* isolate);

namespace temporal {

// #sec-temporal-createtemporalinstant
V8_WARN_UNUSED_RESULT MaybeDirectHandle<JSTemporalInstant>
CreateTemporalInstantWithValidityCheck(Isolate* isolate,
                                       DirectHandle<JSFunction> target,
                                       DirectHandle<HeapObject> new_target,
                                       DirectHandle<BigInt> epoch_nanoseconds);
V8_WARN_UNUSED_RESULT MaybeDirectHandle<JSTemporalInstant>
CreateTemporalInstantWithValidityCheck(Isolate* isolate,
                                       DirectHandle<BigInt> epoch_nanoseconds);

// #sec-temporal-builtintimezonegetplaindatetimefor
V8_WARN_UNUSED_RESULT MaybeDirectHandle<JSTemporalPlainDateTime>
BuiltinTimeZoneGetPlainDateTimeFor(Isolate* isolate,
                                   DirectHandle<JSReceiver> time_zone,
                                   DirectHandle<JSTemporalInstant> instant,
                                   DirectHandle<JSReceiver> calendar,
                                   const char* method_name);

V8_WARN_UNUSED_RESULT MaybeDirectHandle<Object> InvokeCalendarMethod(
    Isolate* isolate, DirectHandle<JSReceiver> calendar,
    DirectHandle<String> name, DirectHandle<JSReceiver> temporal_like);

// Used in DurationFormat
V8_WARN_UNUSED_RESULT Maybe<DurationRecord> ToTemporalDurationAsRecord(
    Isolate* isolate, DirectHandle<Object> item, const char* method_name);

}  // namespace temporal
}  // namespace internal
}  // namespace v8
#include "src/objects/object-macros-undef.h"
#endif  // V8_OBJECTS_JS_TEMPORAL_OBJECTS_H_
