// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/objects/bigint.h"
#include "src/objects/js-temporal-objects-inl.h"

namespace v8 {
namespace internal {

#define TEMPORAL_NOW0(METHOD, cpp)                   \
  BUILTIN(TemporalNow##METHOD) {                     \
    HandleScope scope(isolate);                      \
    RETURN_RESULT_OR_FAILURE(isolate, cpp(isolate)); \
  }

#define TEMPORAL_NOW_ISO1(T)                                             \
  BUILTIN(TemporalNow##T##ISO) {                                         \
    HandleScope scope(isolate);                                          \
    RETURN_RESULT_OR_FAILURE(                                            \
        isolate,                                                         \
        JSTemporal##T::NowISO(isolate, args.atOrUndefined(isolate, 1))); \
  }

#define TEMPORAL_CONSTRUCTOR1(T)                                              \
  BUILTIN(Temporal##T##Constructor) {                                         \
    HandleScope scope(isolate);                                               \
    RETURN_RESULT_OR_FAILURE(                                                 \
        isolate,                                                              \
        JSTemporal##T::Constructor(isolate, args.target(), args.new_target(), \
                                   args.atOrUndefined(isolate, 1)));          \
  }

#define TEMPORAL_PROTOTYPE_METHOD0(T, METHOD, name)                          \
  BUILTIN(Temporal##T##Prototype##METHOD) {                                  \
    HandleScope scope(isolate);                                              \
    CHECK_RECEIVER(JSTemporal##T, obj, "Temporal." #T ".prototype." #name);  \
    RETURN_RESULT_OR_FAILURE(isolate, JSTemporal##T ::METHOD(isolate, obj)); \
  }

#define TEMPORAL_PROTOTYPE_METHOD1(T, METHOD, name)                            \
  BUILTIN(Temporal##T##Prototype##METHOD) {                                    \
    HandleScope scope(isolate);                                                \
    CHECK_RECEIVER(JSTemporal##T, obj, "Temporal." #T ".prototype." #name);    \
    RETURN_RESULT_OR_FAILURE(                                                  \
        isolate,                                                               \
        JSTemporal##T ::METHOD(isolate, obj, args.atOrUndefined(isolate, 1))); \
  }

#define TEMPORAL_PROTOTYPE_METHOD2(T, METHOD, name)                          \
  BUILTIN(Temporal##T##Prototype##METHOD) {                                  \
    HandleScope scope(isolate);                                              \
    CHECK_RECEIVER(JSTemporal##T, obj, "Temporal." #T ".prototype." #name);  \
    RETURN_RESULT_OR_FAILURE(                                                \
        isolate,                                                             \
        JSTemporal##T ::METHOD(isolate, obj, args.atOrUndefined(isolate, 1), \
                               args.atOrUndefined(isolate, 2)));             \
  }

#define TEMPORAL_PROTOTYPE_METHOD3(T, METHOD, name)                          \
  BUILTIN(Temporal##T##Prototype##METHOD) {                                  \
    HandleScope scope(isolate);                                              \
    CHECK_RECEIVER(JSTemporal##T, obj, "Temporal." #T ".prototype." #name);  \
    RETURN_RESULT_OR_FAILURE(                                                \
        isolate,                                                             \
        JSTemporal##T ::METHOD(isolate, obj, args.atOrUndefined(isolate, 1), \
                               args.atOrUndefined(isolate, 2),               \
                               args.atOrUndefined(isolate, 3)));             \
  }

#define TEMPORAL_METHOD1(T, METHOD)                                       \
  BUILTIN(Temporal##T##METHOD) {                                          \
    HandleScope scope(isolate);                                           \
    RETURN_RESULT_OR_FAILURE(                                             \
        isolate,                                                          \
        JSTemporal##T ::METHOD(isolate, args.atOrUndefined(isolate, 1))); \
  }

#define TEMPORAL_METHOD2(T, METHOD)                                     \
  BUILTIN(Temporal##T##METHOD) {                                        \
    HandleScope scope(isolate);                                         \
    RETURN_RESULT_OR_FAILURE(                                           \
        isolate,                                                        \
        JSTemporal##T ::METHOD(isolate, args.atOrUndefined(isolate, 1), \
                               args.atOrUndefined(isolate, 2)));        \
  }
#define TEMPORAL_METHOD3(T, METHOD)                                     \
  BUILTIN(Temporal##T##METHOD) {                                        \
    HandleScope scope(isolate);                                         \
    RETURN_RESULT_OR_FAILURE(                                           \
        isolate,                                                        \
        JSTemporal##T ::METHOD(isolate, args.atOrUndefined(isolate, 1), \
                               args.atOrUndefined(isolate, 2),          \
                               args.atOrUndefined(isolate, 3)));        \
  }

#define TEMPORAL_VALUE_OF(T)                                                 \
  BUILTIN(Temporal##T##PrototypeValueOf) {                                   \
    HandleScope scope(isolate);                                              \
    THROW_NEW_ERROR_RETURN_FAILURE(                                          \
        isolate, NewTypeError(MessageTemplate::kDoNotUse,                    \
                              isolate->factory()->NewStringFromAsciiChecked( \
                                  "Temporal." #T ".prototype.valueOf"),      \
                              isolate->factory()->NewStringFromAsciiChecked( \
                                  "use Temporal." #T                         \
                                  ".prototype.compare for comparison.")));   \
  }

// Like TEMPORAL_GET, but gets from an underlying Rust function
// rust_field is the name of the field with the Rust type. rust_getter is the
// name of the getter on the rust side (ideally the same as `field`). cvt is
// conversion code that converts `value` into the final returned JS Handle (use
// one of the macros below)
#define TEMPORAL_GET_RUST(T, METHOD, js_field, rust_getter, cvt) \
  BUILTIN(Temporal##T##Prototype##METHOD) {                      \
    HandleScope scope(isolate);                                  \
    CHECK_RECEIVER(JSTemporal##T, obj,                           \
                   "Temporal." #T ".prototype." #js_field);      \
    auto rust = obj->wrapped_rust();                             \
    auto value = rust->rust_getter();                            \
    cvt                                                          \
  }

#define CONVERT_INTEGER64 return *isolate->factory()->NewNumberFromInt64(value);
#define CONVERT_SMI return Smi::FromInt(value);
#define CONVERT_BOOLEAN return *isolate->factory()->ToBoolean(value);
#define CONVERT_DOUBLE return *isolate->factory()->NewNumber(value);
#define CONVERT_ASCII_STRING \
  return *isolate->factory()->NewStringFromAsciiChecked(value);

// Converts empty to undefined (temporal_capi returns empty era codes when
// undefined)
#define CONVERT_NULLABLE_ASCII_STRING                             \
  if (!value.empty()) {                                           \
    return *isolate->factory()->NewStringFromAsciiChecked(value); \
  } else {                                                        \
    return *isolate->factory()->undefined_value();                \
  }

// converts nullopt to undefined
#define CONVERT_NULLABLE_INTEGER                          \
  if (value.has_value()) {                                \
    return *isolate->factory()->NewNumber(value.value()); \
  } else {                                                \
    return *isolate->factory()->undefined_value();        \
  }

// temporal_rs returns errors in a couple spots where it should return
// `undefined`
#define CONVERT_FALLIBLE_INTEGER_AS_NULLABLE                              \
  if (value.is_ok()) {                                                    \
    return *isolate->factory()->NewNumber(std::move(value).ok().value()); \
  } else {                                                                \
    return *isolate->factory()->undefined_value();                        \
  }

#define TEMPORAL_GET_NUMBER_AFTER_DIVID(T, M, field, scale, name)        \
  BUILTIN(Temporal##T##Prototype##M) {                                   \
    HandleScope scope(isolate);                                          \
    CHECK_RECEIVER(JSTemporal##T, handle,                                \
                   "get Temporal." #T ".prototype." #name);              \
    DirectHandle<BigInt> value;                                          \
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(                                  \
        isolate, value,                                                  \
        BigInt::Divide(isolate, direct_handle(handle->field(), isolate), \
                       BigInt::FromUint64(isolate, scale)));             \
    DirectHandle<Object> number = BigInt::ToNumber(isolate, value);      \
    DCHECK(std::isfinite(Object::NumberValue(*number)));                 \
    return *number;                                                      \
  }

#define TEMPORAL_GET_BIGINT_AFTER_DIVID(T, M, field, scale, name)        \
  BUILTIN(Temporal##T##Prototype##M) {                                   \
    HandleScope scope(isolate);                                          \
    CHECK_RECEIVER(JSTemporal##T, handle,                                \
                   "get Temporal." #T ".prototype." #name);              \
    RETURN_RESULT_OR_FAILURE(                                            \
        isolate,                                                         \
        BigInt::Divide(isolate, direct_handle(handle->field(), isolate), \
                       BigInt::FromUint64(isolate, scale)));             \
  }

//
// Now
//

// https://tc39.es/proposal-temporal/#sec-temporal.now.instant
TEMPORAL_NOW0(Instant, JSTemporalInstant::Now)
// https://tc39.es/proposal-temporal/#sec-temporal.now.timezoneid
TEMPORAL_NOW0(TimeZoneId, JSTemporalNowTimeZoneId)
// https://tc39.es/proposal-temporal/#sec-temporal.now.plaindatetimeiso
TEMPORAL_NOW_ISO1(PlainDateTime)
// https://tc39.es/proposal-temporal/#sec-temporal.now.plaindateiso
TEMPORAL_NOW_ISO1(PlainDate)
// https://tc39.es/proposal-temporal/#sec-temporal.now.plaintimeiso
TEMPORAL_NOW_ISO1(PlainTime)
// https://tc39.es/proposal-temporal/#sec-temporal.now.zoneddatetimeiso
TEMPORAL_NOW_ISO1(ZonedDateTime)

//
// PlainDate
//

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate
BUILTIN(TemporalPlainDateConstructor) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSTemporalPlainDate::Constructor(
                   isolate, args.target(), args.new_target(),
                   args.atOrUndefined(isolate, 1),    // iso_year
                   args.atOrUndefined(isolate, 2),    // iso_month
                   args.atOrUndefined(isolate, 3),    // iso_day
                   args.atOrUndefined(isolate, 4)));  // calendar_like
}
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.from
TEMPORAL_METHOD2(PlainDate, From)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.compare
TEMPORAL_METHOD2(PlainDate, Compare)

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.calendarid
TEMPORAL_GET_RUST(PlainDate, CalendarId, calendarId, calendar().identifier,
                  CONVERT_ASCII_STRING)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.era
TEMPORAL_GET_RUST(PlainDate, Era, era, era, CONVERT_NULLABLE_ASCII_STRING)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.erayear
TEMPORAL_GET_RUST(PlainDate, EraYear, eraYear, era_year,
                  CONVERT_NULLABLE_INTEGER)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.year
TEMPORAL_GET_RUST(PlainDate, Year, year, year, CONVERT_INTEGER64)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.month
TEMPORAL_GET_RUST(PlainDate, Month, month, month, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.monthcode
TEMPORAL_GET_RUST(PlainDate, MonthCode, monthCode, month_code,
                  CONVERT_ASCII_STRING)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.day
TEMPORAL_GET_RUST(PlainDate, Day, day, day, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.dayofweek
TEMPORAL_GET_RUST(PlainDate, DayOfWeek, dayOfWeek, day_of_week, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.dayofyear
TEMPORAL_GET_RUST(PlainDate, DayOfYear, dayOfYear, day_of_year, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.weekofyear
TEMPORAL_GET_RUST(PlainDate, WeekOfYear, weekOfYear, week_of_year,
                  CONVERT_NULLABLE_INTEGER)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.yearofweek
TEMPORAL_GET_RUST(PlainDate, YearOfWeek, YearOfWeek, year_of_week,
                  CONVERT_NULLABLE_INTEGER)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.daysinweek
TEMPORAL_GET_RUST(PlainDate, DaysInWeek, daysInWeek, days_in_week, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.daysinmonth
TEMPORAL_GET_RUST(PlainDate, DaysInMonth, daysInMonth, days_in_month,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.daysinyear
TEMPORAL_GET_RUST(PlainDate, DaysInYear, daysInYear, days_in_year, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.monthsinyear
TEMPORAL_GET_RUST(PlainDate, MonthsInYear, monthsInYear, months_in_year,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindate.prototype.inleapyear
TEMPORAL_GET_RUST(PlainDate, InLeapYear, inLeapYear, in_leap_year,
                  CONVERT_BOOLEAN)

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.toplainyearmonth
TEMPORAL_PROTOTYPE_METHOD0(PlainDate, ToPlainYearMonth, toPlainYearMonth)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.toplainmonthday
TEMPORAL_PROTOTYPE_METHOD0(PlainDate, ToPlainMonthDay, toPlainMonthDay)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.add
TEMPORAL_PROTOTYPE_METHOD2(PlainDate, Add, add)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.substract
TEMPORAL_PROTOTYPE_METHOD2(PlainDate, Subtract, subtract)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.with
TEMPORAL_PROTOTYPE_METHOD2(PlainDate, With, with)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.withcalendar
TEMPORAL_PROTOTYPE_METHOD1(PlainDate, WithCalendar, withCalendar)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.until
TEMPORAL_PROTOTYPE_METHOD2(PlainDate, Until, until)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.since
TEMPORAL_PROTOTYPE_METHOD2(PlainDate, Since, since)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.equals
TEMPORAL_PROTOTYPE_METHOD1(PlainDate, Equals, equals)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.toplaindatetime
TEMPORAL_PROTOTYPE_METHOD1(PlainDate, ToPlainDateTime, toPlainDateTime)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tozoneddatetime
TEMPORAL_PROTOTYPE_METHOD1(PlainDate, ToZonedDateTime, toZonedDateTime)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tostring
TEMPORAL_PROTOTYPE_METHOD1(PlainDate, ToString, toString)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tolocalestring
TEMPORAL_PROTOTYPE_METHOD2(PlainDate, ToLocaleString, toLocaleString)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tojson
TEMPORAL_PROTOTYPE_METHOD0(PlainDate, ToJSON, toJSON)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.valueof
TEMPORAL_VALUE_OF(PlainDate)

//
// PlainTime
//

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime
BUILTIN(TemporalPlainTimeConstructor) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(isolate,
                           JSTemporalPlainTime::Constructor(
                               isolate, args.target(), args.new_target(),
                               args.atOrUndefined(isolate, 1),    // hour
                               args.atOrUndefined(isolate, 2),    // minute
                               args.atOrUndefined(isolate, 3),    // second
                               args.atOrUndefined(isolate, 4),    // millisecond
                               args.atOrUndefined(isolate, 5),    // microsecond
                               args.atOrUndefined(isolate, 6)));  // nanosecond
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.from
TEMPORAL_METHOD2(PlainTime, From)
// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.compare
TEMPORAL_METHOD2(PlainTime, Compare)

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaintime.prototype.hour
TEMPORAL_GET_RUST(PlainTime, Hour, hour, hour, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaintime.prototype.minute
TEMPORAL_GET_RUST(PlainTime, Minute, minute, minute, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaintime.prototype.second
TEMPORAL_GET_RUST(PlainTime, Second, second, second, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaintime.prototype.millisecond
TEMPORAL_GET_RUST(PlainTime, Millisecond, millisecond, millisecond, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaintime.prototype.microsecond
TEMPORAL_GET_RUST(PlainTime, Microsecond, microsecond, microsecond, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaintime.prototype.nanosecond
TEMPORAL_GET_RUST(PlainTime, Nanosecond, nanosecond, nanosecond, CONVERT_SMI)

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.add
TEMPORAL_PROTOTYPE_METHOD1(PlainTime, Add, add)
// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.subtract
TEMPORAL_PROTOTYPE_METHOD1(PlainTime, Subtract, subtract)
// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.with
TEMPORAL_PROTOTYPE_METHOD2(PlainTime, With, with)
// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.until
TEMPORAL_PROTOTYPE_METHOD2(PlainTime, Until, until)
// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.since
TEMPORAL_PROTOTYPE_METHOD2(PlainTime, Since, since)
// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.round
TEMPORAL_PROTOTYPE_METHOD1(PlainTime, Round, round)
// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.equals
TEMPORAL_PROTOTYPE_METHOD1(PlainTime, Equals, equals)
// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.tostring
TEMPORAL_PROTOTYPE_METHOD1(PlainTime, ToString, toString)
// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.tolocalestring
TEMPORAL_PROTOTYPE_METHOD2(PlainTime, ToLocaleString, toLocaleString)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindtimeprototype.tojson
TEMPORAL_PROTOTYPE_METHOD0(PlainTime, ToJSON, toJSON)
// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.valueof
TEMPORAL_VALUE_OF(PlainTime)

//
// PlainDateTime
//

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime
BUILTIN(TemporalPlainDateTimeConstructor) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSTemporalPlainDateTime::Constructor(
                   isolate, args.target(), args.new_target(),
                   args.atOrUndefined(isolate, 1),     // iso_year
                   args.atOrUndefined(isolate, 2),     // iso_month
                   args.atOrUndefined(isolate, 3),     // iso_day
                   args.atOrUndefined(isolate, 4),     // hour
                   args.atOrUndefined(isolate, 5),     // minute
                   args.atOrUndefined(isolate, 6),     // second
                   args.atOrUndefined(isolate, 7),     // millisecond
                   args.atOrUndefined(isolate, 8),     // microsecond
                   args.atOrUndefined(isolate, 9),     // nanosecond
                   args.atOrUndefined(isolate, 10)));  // calendar_like
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.from
TEMPORAL_METHOD2(PlainDateTime, From)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.compare
TEMPORAL_METHOD2(PlainDateTime, Compare)

// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.calendarid
TEMPORAL_GET_RUST(PlainDateTime, CalendarId, calendarId, calendar().identifier,
                  CONVERT_ASCII_STRING)
TEMPORAL_GET_RUST(PlainDateTime, Year, year, year, CONVERT_INTEGER64)
TEMPORAL_GET_RUST(PlainDateTime, Era, era, era, CONVERT_NULLABLE_ASCII_STRING)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.erayear
TEMPORAL_GET_RUST(PlainDateTime, EraYear, eraYear, era_year,
                  CONVERT_NULLABLE_INTEGER)
TEMPORAL_GET_RUST(PlainDateTime, Month, month, month, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.monthcode
TEMPORAL_GET_RUST(PlainDateTime, MonthCode, monthCode, month_code,
                  CONVERT_ASCII_STRING)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.day
TEMPORAL_GET_RUST(PlainDateTime, Day, day, day, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.hour
TEMPORAL_GET_RUST(PlainDateTime, Hour, hour, hour, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.minute
TEMPORAL_GET_RUST(PlainDateTime, Minute, minute, minute, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.second
TEMPORAL_GET_RUST(PlainDateTime, Second, second, second, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.millisecond
TEMPORAL_GET_RUST(PlainDateTime, Millisecond, millisecond, millisecond,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.microsecond
TEMPORAL_GET_RUST(PlainDateTime, Microsecond, microsecond, microsecond,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.nanosecond
TEMPORAL_GET_RUST(PlainDateTime, Nanosecond, nanosecond, nanosecond,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.dayofweek
TEMPORAL_GET_RUST(PlainDateTime, DayOfWeek, dayOfWeek, day_of_week, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.dayofyear
TEMPORAL_GET_RUST(PlainDateTime, DayOfYear, dayOfYear, day_of_year, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.weekofyear
TEMPORAL_GET_RUST(PlainDateTime, WeekOfYear, weekOfYear, week_of_year,
                  CONVERT_NULLABLE_INTEGER)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.yearofweek
TEMPORAL_GET_RUST(PlainDateTime, YearOfWeek, YearOfWeek, year_of_week,
                  CONVERT_NULLABLE_INTEGER)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.daysinweek
TEMPORAL_GET_RUST(PlainDateTime, DaysInWeek, daysInWeek, days_in_week,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.daysinmonth
TEMPORAL_GET_RUST(PlainDateTime, DaysInMonth, daysInMonth, days_in_month,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.daysinyear
TEMPORAL_GET_RUST(PlainDateTime, DaysInYear, daysInYear, days_in_year,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.monthsinyear
TEMPORAL_GET_RUST(PlainDateTime, MonthsInYear, monthsInYear, months_in_year,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plaindatetime.prototype.inleapyear
TEMPORAL_GET_RUST(PlainDateTime, InLeapYear, inLeapYear, in_leap_year,
                  CONVERT_BOOLEAN)

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.with
TEMPORAL_PROTOTYPE_METHOD2(PlainDateTime, With, with)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.withplainTime
TEMPORAL_PROTOTYPE_METHOD1(PlainDateTime, WithPlainTime, withPlainTime)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.withcalendar
TEMPORAL_PROTOTYPE_METHOD1(PlainDateTime, WithCalendar, withCalendar)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.add
TEMPORAL_PROTOTYPE_METHOD2(PlainDateTime, Add, add)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.subtract
TEMPORAL_PROTOTYPE_METHOD2(PlainDateTime, Subtract, subtract)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.until
TEMPORAL_PROTOTYPE_METHOD2(PlainDateTime, Until, until)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.since
TEMPORAL_PROTOTYPE_METHOD2(PlainDateTime, Since, since)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.round
TEMPORAL_PROTOTYPE_METHOD1(PlainDateTime, Round, round)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.equals
TEMPORAL_PROTOTYPE_METHOD1(PlainDateTime, Equals, equals)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tostring
TEMPORAL_PROTOTYPE_METHOD1(PlainDateTime, ToString, toString)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tolocalestring
TEMPORAL_PROTOTYPE_METHOD2(PlainDateTime, ToLocaleString, toLocaleString)
// https://tc39.es/proposal-temporal/#sec-temporal.plainddatetimeprototype.tojson
TEMPORAL_PROTOTYPE_METHOD0(PlainDateTime, ToJSON, toJSON)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.valueof
TEMPORAL_VALUE_OF(PlainDateTime)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tozoneddatetime
TEMPORAL_PROTOTYPE_METHOD2(PlainDateTime, ToZonedDateTime, toZonedDateTime)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.toplaindate
TEMPORAL_PROTOTYPE_METHOD0(PlainDateTime, ToPlainDate, toPlainDate)
// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.toplaintime
TEMPORAL_PROTOTYPE_METHOD0(PlainDateTime, ToPlainTime, toPlainTime)

//
// PlainYearMonth
//

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth
BUILTIN(TemporalPlainYearMonthConstructor) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSTemporalPlainYearMonth::Constructor(
                   isolate, args.target(), args.new_target(),
                   args.atOrUndefined(isolate, 1),    // iso_year
                   args.atOrUndefined(isolate, 2),    // iso_month
                   args.atOrUndefined(isolate, 3),    // calendar_like
                   args.atOrUndefined(isolate, 4)));  // reference_iso_day
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.from
TEMPORAL_METHOD2(PlainYearMonth, From)
// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.compare
TEMPORAL_METHOD2(PlainYearMonth, Compare)

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.calendarid
TEMPORAL_GET_RUST(PlainYearMonth, CalendarId, calendarId, calendar().identifier,
                  CONVERT_ASCII_STRING)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.year
TEMPORAL_GET_RUST(PlainYearMonth, Year, year, year, CONVERT_INTEGER64)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.era
TEMPORAL_GET_RUST(PlainYearMonth, Era, era, era, CONVERT_NULLABLE_ASCII_STRING)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.erayear
TEMPORAL_GET_RUST(PlainYearMonth, EraYear, eraYear, era_year,
                  CONVERT_NULLABLE_INTEGER)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.month
TEMPORAL_GET_RUST(PlainYearMonth, Month, month, month, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.monthcode
TEMPORAL_GET_RUST(PlainYearMonth, MonthCode, monthCode, month_code,
                  CONVERT_ASCII_STRING)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.daysinmonth
TEMPORAL_GET_RUST(PlainYearMonth, DaysInMonth, daysInMonth, days_in_month,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.daysinyear
TEMPORAL_GET_RUST(PlainYearMonth, DaysInYear, daysInYear, days_in_year,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.monthsinyear
TEMPORAL_GET_RUST(PlainYearMonth, MonthsInYear, monthsInYear, months_in_year,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.inleapyear
TEMPORAL_GET_RUST(PlainYearMonth, InLeapYear, inLeapYear, in_leap_year,
                  CONVERT_BOOLEAN)

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.with
TEMPORAL_PROTOTYPE_METHOD2(PlainYearMonth, With, with)
// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.add
TEMPORAL_PROTOTYPE_METHOD2(PlainYearMonth, Add, add)
// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.subtract
TEMPORAL_PROTOTYPE_METHOD2(PlainYearMonth, Subtract, subtract)
// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.until
TEMPORAL_PROTOTYPE_METHOD2(PlainYearMonth, Until, until)
// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.since
TEMPORAL_PROTOTYPE_METHOD2(PlainYearMonth, Since, since)
// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.equals
TEMPORAL_PROTOTYPE_METHOD1(PlainYearMonth, Equals, equals)
// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.tostring
TEMPORAL_PROTOTYPE_METHOD1(PlainYearMonth, ToString, toString)
// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.tolocalestring
TEMPORAL_PROTOTYPE_METHOD2(PlainYearMonth, ToLocaleString, toLocaleString)
// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.tojson
TEMPORAL_PROTOTYPE_METHOD0(PlainYearMonth, ToJSON, toJSON)
// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.valueof
TEMPORAL_VALUE_OF(PlainYearMonth)
// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.toplaindate
TEMPORAL_PROTOTYPE_METHOD1(PlainYearMonth, ToPlainDate, toPlainDate)

//
// PlainMonthDay
//

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday
BUILTIN(TemporalPlainMonthDayConstructor) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSTemporalPlainMonthDay::Constructor(
                   isolate, args.target(), args.new_target(),
                   args.atOrUndefined(isolate, 1),    // iso_month
                   args.atOrUndefined(isolate, 2),    // iso_day
                   args.atOrUndefined(isolate, 3),    // calendar_like
                   args.atOrUndefined(isolate, 4)));  // reference_iso_year
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.from
TEMPORAL_METHOD2(PlainMonthDay, From)

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainmonthday.calendarid
TEMPORAL_GET_RUST(PlainMonthDay, CalendarId, calendarId, calendar().identifier,
                  CONVERT_ASCII_STRING)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plainmonthday.prototype.day
TEMPORAL_GET_RUST(PlainMonthDay, Day, day, day, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.plainmonthday.prototype.monthcode
TEMPORAL_GET_RUST(PlainMonthDay, MonthCode, monthCode, month_code,
                  CONVERT_ASCII_STRING)

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.with
TEMPORAL_PROTOTYPE_METHOD2(PlainMonthDay, With, with)
// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.equals
TEMPORAL_PROTOTYPE_METHOD1(PlainMonthDay, Equals, equals)
// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.tostring
TEMPORAL_PROTOTYPE_METHOD1(PlainMonthDay, ToString, toString)
// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.tojson
TEMPORAL_PROTOTYPE_METHOD0(PlainMonthDay, ToJSON, toJSON)
// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.tolocalestring
TEMPORAL_PROTOTYPE_METHOD2(PlainMonthDay, ToLocaleString, toLocaleString)
// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.valueof
TEMPORAL_VALUE_OF(PlainMonthDay)
// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.toplaindate
TEMPORAL_PROTOTYPE_METHOD1(PlainMonthDay, ToPlainDate, toPlainDate)

//
// ZonedDateTime
//

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime
BUILTIN(TemporalZonedDateTimeConstructor) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSTemporalZonedDateTime::Constructor(
                   isolate, args.target(), args.new_target(),
                   args.atOrUndefined(isolate, 1),    // epoch_nanoseconds
                   args.atOrUndefined(isolate, 2),    // time_zone_like
                   args.atOrUndefined(isolate, 3)));  // calendar_like
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.from
TEMPORAL_METHOD2(ZonedDateTime, From)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.compare
TEMPORAL_METHOD2(ZonedDateTime, Compare)

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.calendarid
TEMPORAL_GET_RUST(ZonedDateTime, CalendarId, calendarId, calendar().identifier,
                  CONVERT_ASCII_STRING)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.timezoneid
TEMPORAL_PROTOTYPE_METHOD0(ZonedDateTime, TimeZoneId, time_zone)
TEMPORAL_GET_RUST(ZonedDateTime, Year, year, year, CONVERT_INTEGER64)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.era
TEMPORAL_GET_RUST(ZonedDateTime, Era, era, era, CONVERT_NULLABLE_ASCII_STRING)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.erayear
TEMPORAL_GET_RUST(ZonedDateTime, EraYear, eraYear, era_year,
                  CONVERT_NULLABLE_INTEGER)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.month
TEMPORAL_GET_RUST(ZonedDateTime, Month, month, month, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.monthcode
TEMPORAL_GET_RUST(ZonedDateTime, MonthCode, monthCode, month_code,
                  CONVERT_ASCII_STRING)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.day
TEMPORAL_GET_RUST(ZonedDateTime, Day, day, day, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.hour
TEMPORAL_GET_RUST(ZonedDateTime, Hour, hour, hour, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.minute
TEMPORAL_GET_RUST(ZonedDateTime, Minute, minute, minute, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.second
TEMPORAL_GET_RUST(ZonedDateTime, Second, second, second, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.millisecond
TEMPORAL_GET_RUST(ZonedDateTime, Millisecond, millisecond, millisecond,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.microsecond
TEMPORAL_GET_RUST(ZonedDateTime, Microsecond, microsecond, microsecond,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.nanosecond
TEMPORAL_GET_RUST(ZonedDateTime, Nanosecond, nanosecond, nanosecond,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.epochmilliseconds
TEMPORAL_GET_RUST(ZonedDateTime, EpochMilliseconds, epochMilliseconds,
                  epoch_milliseconds, CONVERT_DOUBLE)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.epochnanoseconds
TEMPORAL_PROTOTYPE_METHOD0(ZonedDateTime, EpochNanoseconds, nanoseconds)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.dayofweek
TEMPORAL_GET_RUST(ZonedDateTime, DayOfWeek, dayOfWeek, day_of_week, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.dayofyear
TEMPORAL_GET_RUST(ZonedDateTime, DayOfYear, dayOfYear, day_of_year, CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.weekofyear
TEMPORAL_GET_RUST(ZonedDateTime, WeekOfYear, weekOfYear, week_of_year,
                  CONVERT_NULLABLE_INTEGER)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.yearofweek
TEMPORAL_GET_RUST(ZonedDateTime, YearOfWeek, YearOfWeek, year_of_week,
                  CONVERT_NULLABLE_INTEGER)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.hoursinday
TEMPORAL_PROTOTYPE_METHOD0(ZonedDateTime, HoursInDay, hoursInDay)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.daysinweek
TEMPORAL_GET_RUST(ZonedDateTime, DaysInWeek, daysInWeek, days_in_week,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.daysinmonth
TEMPORAL_GET_RUST(ZonedDateTime, DaysInMonth, daysInMonth, days_in_month,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.daysinyear
TEMPORAL_GET_RUST(ZonedDateTime, DaysInYear, daysInYear, days_in_year,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.monthsinyear
TEMPORAL_GET_RUST(ZonedDateTime, MonthsInYear, monthsInYear, months_in_year,
                  CONVERT_SMI)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.inleapyear
TEMPORAL_GET_RUST(ZonedDateTime, InLeapYear, inLeapYear, in_leap_year,
                  CONVERT_BOOLEAN)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.offsetnanoseconds
TEMPORAL_PROTOTYPE_METHOD0(ZonedDateTime, OffsetNanoseconds, offsetNanoseconds)
// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.offset
TEMPORAL_PROTOTYPE_METHOD0(ZonedDateTime, Offset, offset)

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.with
TEMPORAL_PROTOTYPE_METHOD2(ZonedDateTime, With, with)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.withplaintime
TEMPORAL_PROTOTYPE_METHOD1(ZonedDateTime, WithPlainTime, withPlainTime)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.withtimezone
TEMPORAL_PROTOTYPE_METHOD1(ZonedDateTime, WithTimeZone, withTimeZone)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.withcalendar
TEMPORAL_PROTOTYPE_METHOD1(ZonedDateTime, WithCalendar, withCalendar)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.add
TEMPORAL_PROTOTYPE_METHOD2(ZonedDateTime, Add, add)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.subtract
TEMPORAL_PROTOTYPE_METHOD2(ZonedDateTime, Subtract, subtract)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.since
TEMPORAL_PROTOTYPE_METHOD2(ZonedDateTime, Since, since)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.until
TEMPORAL_PROTOTYPE_METHOD2(ZonedDateTime, Until, until)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.round
TEMPORAL_PROTOTYPE_METHOD1(ZonedDateTime, Round, round)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.equals
TEMPORAL_PROTOTYPE_METHOD1(ZonedDateTime, Equals, equals)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.tostring
TEMPORAL_PROTOTYPE_METHOD1(ZonedDateTime, ToString, toString)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.tolocalestring
TEMPORAL_PROTOTYPE_METHOD2(ZonedDateTime, ToLocaleString, toLocaleString)
// https://tc39.es/proposal-temporal/#sec-temporal.zonedddatetimeprototype.tojson
TEMPORAL_PROTOTYPE_METHOD0(ZonedDateTime, ToJSON, toJSON)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.valueof
TEMPORAL_VALUE_OF(ZonedDateTime)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.startofday
TEMPORAL_PROTOTYPE_METHOD0(ZonedDateTime, StartOfDay, startOfDay)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.gettimezonetransition
TEMPORAL_PROTOTYPE_METHOD1(ZonedDateTime, GetTimeZoneTransition,
                           getTimeZoneTransition)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toinstant
TEMPORAL_PROTOTYPE_METHOD0(ZonedDateTime, ToInstant, toInstant)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toplaindate
TEMPORAL_PROTOTYPE_METHOD0(ZonedDateTime, ToPlainDate, toPlainDate)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toplaintime
TEMPORAL_PROTOTYPE_METHOD0(ZonedDateTime, ToPlainTime, toPlainTime)
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toplaindatetime
TEMPORAL_PROTOTYPE_METHOD0(ZonedDateTime, ToPlainDateTime, toPlainDateTime)

//
// Duration
//

// https://tc39.es/proposal-temporal/#sec-temporal.duration
BUILTIN(TemporalDurationConstructor) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSTemporalDuration::Constructor(
                   isolate, args.target(), args.new_target(),
                   args.atOrUndefined(isolate, 1),     // years
                   args.atOrUndefined(isolate, 2),     // months
                   args.atOrUndefined(isolate, 3),     // weeks
                   args.atOrUndefined(isolate, 4),     // days
                   args.atOrUndefined(isolate, 5),     // hours
                   args.atOrUndefined(isolate, 6),     // minutes
                   args.atOrUndefined(isolate, 7),     // seconds
                   args.atOrUndefined(isolate, 8),     // milliseconds
                   args.atOrUndefined(isolate, 9),     // microseconds
                   args.atOrUndefined(isolate, 10)));  // nanoseconds
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.from
TEMPORAL_METHOD1(Duration, From)
// https://tc39.es/proposal-temporal/#sec-temporal.duration.compare
TEMPORAL_METHOD3(Duration, Compare)

// https://tc39.es/proposal-temporal/#sec-get-temporal.duration.prototype.years
TEMPORAL_GET_RUST(Duration, Years, years, years, CONVERT_INTEGER64)
// https://tc39.es/proposal-temporal/#sec-get-temporal.duration.prototype.months
TEMPORAL_GET_RUST(Duration, Months, months, months, CONVERT_INTEGER64)
// https://tc39.es/proposal-temporal/#sec-get-temporal.duration.prototype.weeks
TEMPORAL_GET_RUST(Duration, Weeks, weeks, weeks, CONVERT_INTEGER64)
// https://tc39.es/proposal-temporal/#sec-get-temporal.duration.prototype.days
TEMPORAL_GET_RUST(Duration, Days, days, days, CONVERT_INTEGER64)
// https://tc39.es/proposal-temporal/#sec-get-temporal.duration.prototype.hours
TEMPORAL_GET_RUST(Duration, Hours, hours, hours, CONVERT_INTEGER64)
// https://tc39.es/proposal-temporal/#sec-get-temporal.duration.prototype.minutes
TEMPORAL_GET_RUST(Duration, Minutes, minutes, minutes, CONVERT_INTEGER64)
// https://tc39.es/proposal-temporal/#sec-get-temporal.duration.prototype.seconds
TEMPORAL_GET_RUST(Duration, Seconds, seconds, seconds, CONVERT_INTEGER64)
// https://tc39.es/proposal-temporal/#sec-get-temporal.duration.prototype.milliseconds
TEMPORAL_GET_RUST(Duration, Milliseconds, milliseconds, milliseconds,
                  CONVERT_INTEGER64)
// In theory the Duration may have millisecond values that are out of range for
// a float (but in range for a BigInt). Spec asks these functions to be
// converted to a Number so we can just produce Infinity when we are out of
// range.
// https://tc39.es/proposal-temporal/#sec-get-temporal.duration.prototype.microseconds
TEMPORAL_GET_RUST(Duration, Microseconds, microseconds, microseconds,
                  CONVERT_DOUBLE)
// https://tc39.es/proposal-temporal/#sec-get-temporal.duration.prototype.nanoseconds
TEMPORAL_GET_RUST(Duration, Nanoseconds, nanoseconds, nanoseconds,
                  CONVERT_DOUBLE)

// https://tc39.es/proposal-temporal/#sec-get-temporal.duration.prototype.sign
TEMPORAL_PROTOTYPE_METHOD0(Duration, Sign, sign)
// https://tc39.es/proposal-temporal/#sec-get-temporal.duration.prototype.blank
TEMPORAL_PROTOTYPE_METHOD0(Duration, Blank, blank)
// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.with
TEMPORAL_PROTOTYPE_METHOD1(Duration, With, with)
// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.negated
TEMPORAL_PROTOTYPE_METHOD0(Duration, Negated, negated)
// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.abs
TEMPORAL_PROTOTYPE_METHOD0(Duration, Abs, abs)
// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.add
TEMPORAL_PROTOTYPE_METHOD2(Duration, Add, add)
// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.subtract
TEMPORAL_PROTOTYPE_METHOD2(Duration, Subtract, subtract)
// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.round
TEMPORAL_PROTOTYPE_METHOD1(Duration, Round, round)
// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.total
TEMPORAL_PROTOTYPE_METHOD1(Duration, Total, total)
// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.tostring
TEMPORAL_PROTOTYPE_METHOD1(Duration, ToString, toString)
// https://tc39.es/proposal-temporal/#sec-temporal.duration.tojson
TEMPORAL_PROTOTYPE_METHOD0(Duration, ToJSON, toJSON)
// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.tolocalestring
TEMPORAL_PROTOTYPE_METHOD2(Duration, ToLocaleString, toLocaleString)
// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.valueof
TEMPORAL_VALUE_OF(Duration)

// Instant
// https://tc39.es/proposal-temporal/#sec-temporal.instant
TEMPORAL_CONSTRUCTOR1(Instant)
// https://tc39.es/proposal-temporal/#sec-temporal.instant.from
TEMPORAL_METHOD1(Instant, From)
// https://tc39.es/proposal-temporal/#sec-temporal.instant.fromepochmilliseconds
TEMPORAL_METHOD1(Instant, FromEpochMilliseconds)
// https://tc39.es/proposal-temporal/#sec-temporal.instant.fromepochnanoseconds
TEMPORAL_METHOD1(Instant, FromEpochNanoseconds)
// https://tc39.es/proposal-temporal/#sec-temporal.instant.compare
TEMPORAL_METHOD2(Instant, Compare)
// https://tc39.es/proposal-temporal/#sec-get-temporal.instant.prototype.epochnanoseconds
TEMPORAL_PROTOTYPE_METHOD0(Instant, EpochNanoseconds, epochNanoseconds)
// https://tc39.es/proposal-temporal/#sec-get-temporal.instant.prototype.epochmilliseconds
TEMPORAL_PROTOTYPE_METHOD0(Instant, EpochMilliseconds, epochMilliseconds)
// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.add
TEMPORAL_PROTOTYPE_METHOD1(Instant, Add, add)
// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.subtract
TEMPORAL_PROTOTYPE_METHOD1(Instant, Subtract, subtract)
// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.until
TEMPORAL_PROTOTYPE_METHOD2(Instant, Until, until)
// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.since
TEMPORAL_PROTOTYPE_METHOD2(Instant, Since, since)
// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.round
TEMPORAL_PROTOTYPE_METHOD1(Instant, Round, round)
// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.equals
TEMPORAL_PROTOTYPE_METHOD1(Instant, Equals, equals)
// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.tostring
TEMPORAL_PROTOTYPE_METHOD1(Instant, ToString, toString)
// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.tolocalestring
TEMPORAL_PROTOTYPE_METHOD2(Instant, ToLocaleString, toLocaleString)
// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.tojson
TEMPORAL_PROTOTYPE_METHOD0(Instant, ToJSON, toJSON)
// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.valueof
TEMPORAL_VALUE_OF(Instant)
// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.tozoneddatetimeiso
TEMPORAL_PROTOTYPE_METHOD1(Instant, ToZonedDateTimeISO, toZonedDateTimeISO)

}  // namespace internal
}  // namespace v8
