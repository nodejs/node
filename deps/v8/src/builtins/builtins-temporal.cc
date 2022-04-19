// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/objects/bigint.h"
#include "src/objects/js-temporal-objects-inl.h"

namespace v8 {
namespace internal {

#define TO_BE_IMPLEMENTED(id)   \
  BUILTIN_NO_RCS(id) {          \
    HandleScope scope(isolate); \
    UNIMPLEMENTED();            \
  }

#define TEMPORAL_NOW0(T)                                            \
  BUILTIN(TemporalNow##T) {                                         \
    HandleScope scope(isolate);                                     \
    RETURN_RESULT_OR_FAILURE(isolate, JSTemporal##T::Now(isolate)); \
  }

#define TEMPORAL_NOW2(T)                                                     \
  BUILTIN(TemporalNow##T) {                                                  \
    HandleScope scope(isolate);                                              \
    RETURN_RESULT_OR_FAILURE(                                                \
        isolate, JSTemporal##T::Now(isolate, args.atOrUndefined(isolate, 1), \
                                    args.atOrUndefined(isolate, 2)));        \
  }

#define TEMPORAL_NOW_ISO1(T)                                             \
  BUILTIN(TemporalNow##T##ISO) {                                         \
    HandleScope scope(isolate);                                          \
    RETURN_RESULT_OR_FAILURE(                                            \
        isolate,                                                         \
        JSTemporal##T::NowISO(isolate, args.atOrUndefined(isolate, 1))); \
  }

/* Temporal #sec-temporal.plaindate.compare */
TO_BE_IMPLEMENTED(TemporalPlainDateCompare)
/* Temporal #sec-temporal.plaindate.prototype.toplainyearmonth */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeToPlainYearMonth)
/* Temporal #sec-temporal.plaindate.prototype.toplainmonthday */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeToPlainMonthDay)
/* Temporal #sec-temporal.plaindate.prototype.add */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeAdd)
/* Temporal #sec-temporal.plaindate.prototype.substract */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeSubtract)
/* Temporal #sec-temporal.plaindate.prototype.with */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeWith)
/* Temporal #sec-temporal.plaindate.prototype.until */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeUntil)
/* Temporal #sec-temporal.plaindate.prototype.since */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeSince)
/* Temporal #sec-temporal.plaindate.prototype.equals */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeEquals)
/* Temporal #sec-temporal.plaindate.prototype.toplaindatetime */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeToPlainDateTime)
/* Temporal #sec-temporal.plaindate.prototype.tozoneddatetime */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeToZonedDateTime)
/* Temporal #sec-temporal.plaindate.prototype.tostring */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeToString)
/* Temporal #sec-temporal.plaindate.prototype.tojson */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeToJSON)

/* Temporal.PlaneTime */
/* Temporal #sec-temporal.plaintime.compare */
TO_BE_IMPLEMENTED(TemporalPlainTimeCompare)
/* Temporal #sec-temporal.plaintime.prototype.add */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeAdd)
/* Temporal #sec-temporal.plaintime.prototype.subtract */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeSubtract)
/* Temporal #sec-temporal.plaintime.prototype.with */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeWith)
/* Temporal #sec-temporal.plaintime.prototype.until */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeUntil)
/* Temporal #sec-temporal.plaintime.prototype.since */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeSince)
/* Temporal #sec-temporal.plaintime.prototype.round */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeRound)
/* Temporal #sec-temporal.plaintime.prototype.equals */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeEquals)
/* Temporal #sec-temporal.plaintime.prototype.toplaindatetime */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeToPlainDateTime)
/* Temporal #sec-temporal.plaintime.prototype.tozoneddatetime */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeToZonedDateTime)
/* Temporal #sec-temporal.plaintime.prototype.tostring */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeToString)
/* Temporal #sec-temporal.plaindtimeprototype.tojson */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeToJSON)

/* Temporal.PlaneDateTime */
/* Temporal #sec-temporal.plaindatetime.from */
TO_BE_IMPLEMENTED(TemporalPlainDateTimeFrom)
/* Temporal #sec-temporal.plaindatetime.compare */
TO_BE_IMPLEMENTED(TemporalPlainDateTimeCompare)
/* Temporal #sec-temporal.plaindatetime.prototype.with */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeWith)
/* Temporal #sec-temporal.plaindatetime.prototype.withplainTime */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeWithPlainTime)
/* Temporal #sec-temporal.plaindatetime.prototype.withplainDate */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeWithPlainDate)
/* Temporal #sec-temporal.plaindatetime.prototype.add */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeAdd)
/* Temporal #sec-temporal.plaindatetime.prototype.subtract */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeSubtract)
/* Temporal #sec-temporal.plaindatetime.prototype.until */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeUntil)
/* Temporal #sec-temporal.plaindatetime.prototype.since */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeSince)
/* Temporal #sec-temporal.plaindatetime.prototype.round */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeRound)
/* Temporal #sec-temporal.plaindatetime.prototype.equals */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeEquals)
/* Temporal #sec-temporal.plaindatetime.prototype.tostring */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeToString)
/* Temporal #sec-temporal.plainddatetimeprototype.tojson */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeToJSON)
/* Temporal #sec-temporal.plaindatetime.prototype.tozoneddatetime */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeToZonedDateTime)
/* Temporal #sec-temporal.plaindatetime.prototype.toplaindate */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeToPlainDate)
/* Temporal #sec-temporal.plaindatetime.prototype.toplainyearmonth */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeToPlainYearMonth)
/* Temporal #sec-temporal.plaindatetime.prototype.toplainmonthday */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeToPlainMonthDay)
/* Temporal #sec-temporal.plaindatetime.prototype.toplaintime */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeToPlainTime)

/* Temporal.ZonedDateTime */
/* Temporal #sec-temporal.zoneddatetime.from */
TO_BE_IMPLEMENTED(TemporalZonedDateTimeFrom)
/* Temporal #sec-temporal.zoneddatetime.compare */
TO_BE_IMPLEMENTED(TemporalZonedDateTimeCompare)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.year */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeYear)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.month */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeMonth)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.monthcode */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeMonthCode)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.day */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeDay)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.dayofweek */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeDayOfWeek)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.dayofyear */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeDayOfYear)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.weekofyear */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeWeekOfYear)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.hoursinday */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeHoursInDay)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.daysinweek */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeDaysInWeek)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.daysinmonth */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeDaysInMonth)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.daysinyear */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeDaysInYear)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.monthsinyear */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeMonthsInYear)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.inleapyear */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeInLeapYear)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.offsetnanoseconds */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeOffsetNanoseconds)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.offset */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeOffset)
/* Temporal #sec-temporal.zoneddatetime.prototype.with */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeWith)
/* Temporal #sec-temporal.zoneddatetime.prototype.withplaintime */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeWithPlainTime)
/* Temporal #sec-temporal.zoneddatetime.prototype.withplaindate */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeWithPlainDate)
/* Temporal #sec-temporal.zoneddatetime.prototype.add */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeAdd)
/* Temporal #sec-temporal.zoneddatetime.prototype.subtract */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeSubtract)
/* Temporal #sec-temporal.zoneddatetime.prototype.until */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeUntil)
/* Temporal #sec-temporal.zoneddatetime.prototype.since */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeSince)
/* Temporal #sec-temporal.zoneddatetime.prototype.round */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeRound)
/* Temporal #sec-temporal.zoneddatetime.prototype.equals */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeEquals)
/* Temporal #sec-temporal.zoneddatetime.prototype.tostring */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToString)
/* Temporal #sec-temporal.zonedddatetimeprototype.tojson */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToJSON)
/* Temporal #sec-temporal.zoneddatetime.prototype.startofday */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeStartOfDay)
/* Temporal #sec-temporal.zoneddatetime.prototype.toinstant */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToInstant)
/* Temporal #sec-temporal.zoneddatetime.prototype.toplaindate */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToPlainDate)
/* Temporal #sec-temporal.zoneddatetime.prototype.toplaintime */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToPlainTime)
/* Temporal #sec-temporal.zoneddatetime.prototype.toplaindatetime */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToPlainDateTime)

/* Temporal.Duration */
/* Temporal #sec-temporal.duration.from */
TO_BE_IMPLEMENTED(TemporalDurationFrom)
/* Temporal #sec-temporal.duration.compare */
TO_BE_IMPLEMENTED(TemporalDurationCompare)
/* Temporal #sec-temporal.duration.prototype.with */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeWith)
/* Temporal #sec-temporal.duration.prototype.add */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeAdd)
/* Temporal #sec-temporal.duration.prototype.subtract */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeSubtract)
/* Temporal #sec-temporal.duration.prototype.round */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeRound)
/* Temporal #sec-temporal.duration.prototype.total */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeTotal)
/* Temporal #sec-temporal.duration.prototype.tostring */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeToString)
/* Temporal #sec-temporal.duration.tojson */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeToJSON)

/* Temporal.Instant */
/* Temporal #sec-temporal.instant.compare */
TO_BE_IMPLEMENTED(TemporalInstantCompare)
/* Temporal #sec-temporal.instant.prototype.add */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeAdd)
/* Temporal #sec-temporal.instant.prototype.subtract */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeSubtract)
/* Temporal #sec-temporal.instant.prototype.until */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeUntil)
/* Temporal #sec-temporal.instant.prototype.since */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeSince)
/* Temporal #sec-temporal.instant.prototype.round */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeRound)
/* Temporal #sec-temporal.instant.prototype.equals */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeEquals)
/* Temporal #sec-temporal.instant.prototype.tostring */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeToString)
/* Temporal #sec-temporal.instant.tojson */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeToJSON)
/* Temporal #sec-temporal.instant.prototype.tozoneddatetime */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeToZonedDateTime)
/* Temporal #sec-temporal.instant.prototype.tozoneddatetimeiso */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeToZonedDateTimeISO)

/* Temporal.PlainYearMonth */
/* Temporal #sec-temporal.plainyearmonth.from */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthFrom)
/* Temporal #sec-temporal.plainyearmonth.compare */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthCompare)
/* Temporal #sec-temporal.plainyearmonth.prototype.with */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeWith)
/* Temporal #sec-temporal.plainyearmonth.prototype.add */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeAdd)
/* Temporal #sec-temporal.plainyearmonth.prototype.subtract */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeSubtract)
/* Temporal #sec-temporal.plainyearmonth.prototype.until */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeUntil)
/* Temporal #sec-temporal.plainyearmonth.prototype.since */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeSince)
/* Temporal #sec-temporal.plainyearmonth.prototype.equals */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeEquals)
/* Temporal #sec-temporal.plainyearmonth.tostring */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeToString)
/* Temporal #sec-temporal.plainyearmonth.tojson */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeToJSON)
/* Temporal #sec-temporal.plainyearmonth.prototype.toplaindate */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeToPlainDate)

/* Temporal.PlainMonthDay */
/* Temporal #sec-temporal.plainmonthday.from */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayFrom)

/* There is no compare for PlainMonthDay. See
 * https://github.com/tc39/proposal-temporal/issues/1547 */

/* Temporal #sec-temporal.plainmonthday.prototype.with */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeWith)
/* Temporal #sec-temporal.plainmonthday.prototype.equals */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeEquals)
/* Temporal #sec-temporal.plainmonthday.prototype.tostring */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeToString)
/* Temporal #sec-temporal.plainmonthday.tojson */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeToJSON)
/* Temporal #sec-temporal.plainmonthday.prototype.toplaindate */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeToPlainDate)

/* Temporal.TimeZone */
/* Temporal #sec-temporal.timezone.prototype.getoffsetnanosecondsfor */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeGetOffsetNanosecondsFor)
/* Temporal #sec-temporal.timezone.prototype.getoffsetstringfor */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeGetOffsetStringFor)
/* Temporal #sec-temporal.timezone.prototype.getplaindatetimefor */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeGetPlainDateTimeFor)
/* Temporal #sec-temporal.timezone.prototype.getinstantfor */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeGetInstantFor)
/* Temporal #sec-temporal.timezone.prototype.getpossibleinstantsfor */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeGetPossibleInstantsFor)
/* Temporal #sec-temporal.timezone.prototype.getnexttransition */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeGetNextTransition)
/* Temporal #sec-temporal.timezone.prototype.getprevioustransition */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeGetPreviousTransition)
/* Temporal #sec-temporal.timezone.prototype.tojson */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeToJSON)

/* Temporal.Calendar */
/* Temporal #sec-temporal.calendar.prototype.yearmonthfromfields */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeYearMonthFromFields)
/* Temporal #sec-temporal.calendar.prototype.monthdayfromfields */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeMonthDayFromFields)
/* Temporal #sec-temporal.calendar.prototype.dateadd */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDateAdd)
/* Temporal #sec-temporal.calendar.prototype.dateuntil */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDateUntil)
/* Temporal #sec-temporal.calendar.prototype.month */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeMonth)
/* Temporal #sec-temporal.calendar.prototype.monthcode */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeMonthCode)
/* Temporal #sec-temporal.calendar.prototype.day */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDay)
/* Temporal #sec-temporal.calendar.prototype.weekofyear */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeWeekOfYear)
/* Temporal #sec-temporal.calendar.prototype.tojson */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeToJSON)

// to be switch to TFJ later
/* Temporal #sec-temporal.calendar.prototype.fields */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeFields)

#ifdef V8_INTL_SUPPORT
/* Temporal */
/* Temporal #sec-temporal.calendar.prototype.era */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeEra)
/* Temporal #sec-temporal.calendar.prototype.erayear */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeEraYear)
/* Temporal #sec-temporal.duration.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeToLocaleString)
/* Temporal #sec-temporal.instant.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeToLocaleString)
/* Temporal #sec-temporal.plaindate.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeToLocaleString)
/* Temporal #sec-temporal.plaindatetime.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeToLocaleString)
/* Temporal #sec-temporal.plainmonthday.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeToLocaleString)
/* Temporal #sec-temporal.plaintime.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeToLocaleString)
/* Temporal #sec-temporal.plainyearmonth.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeToLocaleString)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.era */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeEra)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.erayear */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeEraYear)
/* Temporal #sec-temporal.zoneddatetime.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToLocaleString)
#endif  // V8_INTL_SUPPORT

#define TEMPORAL_CONSTRUCTOR1(T)                                              \
  BUILTIN(Temporal##T##Constructor) {                                         \
    HandleScope scope(isolate);                                               \
    RETURN_RESULT_OR_FAILURE(                                                 \
        isolate,                                                              \
        JSTemporal##T::Constructor(isolate, args.target(), args.new_target(), \
                                   args.atOrUndefined(isolate, 1)));          \
  }

#define TEMPORAL_ID_BY_TO_STRING(T)                               \
  BUILTIN(Temporal##T##PrototypeId) {                             \
    HandleScope scope(isolate);                                   \
    Handle<String> id;                                            \
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(                           \
        isolate, id, Object::ToString(isolate, args.receiver())); \
    return *id;                                                   \
  }

#define TEMPORAL_TO_STRING(T)                                       \
  BUILTIN(Temporal##T##PrototypeToString) {                         \
    HandleScope scope(isolate);                                     \
    const char* method = "Temporal." #T ".prototype.toString";      \
    CHECK_RECEIVER(JSTemporal##T, t, method);                       \
    Handle<Object> ret;                                             \
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(                             \
        isolate, ret, JSTemporal##T::ToString(isolate, t, method)); \
    return *ret;                                                    \
  }

#define TEMPORAL_PROTOTYPE_METHOD0(T, METHOD, name)                          \
  BUILTIN(Temporal##T##Prototype##METHOD) {                                  \
    HandleScope scope(isolate);                                              \
    const char* method = "Temporal." #T ".prototype." #name;                 \
    CHECK_RECEIVER(JSTemporal##T, obj, method);                              \
    RETURN_RESULT_OR_FAILURE(isolate, JSTemporal##T ::METHOD(isolate, obj)); \
  }

#define TEMPORAL_PROTOTYPE_METHOD1(T, METHOD, name)                            \
  BUILTIN(Temporal##T##Prototype##METHOD) {                                    \
    HandleScope scope(isolate);                                                \
    const char* method = "Temporal." #T ".prototype." #name;                   \
    CHECK_RECEIVER(JSTemporal##T, obj, method);                                \
    RETURN_RESULT_OR_FAILURE(                                                  \
        isolate,                                                               \
        JSTemporal##T ::METHOD(isolate, obj, args.atOrUndefined(isolate, 1))); \
  }

#define TEMPORAL_PROTOTYPE_METHOD2(T, METHOD, name)                          \
  BUILTIN(Temporal##T##Prototype##METHOD) {                                  \
    HandleScope scope(isolate);                                              \
    const char* method = "Temporal." #T ".prototype." #name;                 \
    CHECK_RECEIVER(JSTemporal##T, obj, method);                              \
    RETURN_RESULT_OR_FAILURE(                                                \
        isolate,                                                             \
        JSTemporal##T ::METHOD(isolate, obj, args.atOrUndefined(isolate, 1), \
                               args.atOrUndefined(isolate, 2)));             \
  }

#define TEMPORAL_PROTOTYPE_METHOD3(T, METHOD, name)                          \
  BUILTIN(Temporal##T##Prototype##METHOD) {                                  \
    HandleScope scope(isolate);                                              \
    const char* method = "Temporal." #T ".prototype." #name;                 \
    CHECK_RECEIVER(JSTemporal##T, obj, method);                              \
    RETURN_RESULT_OR_FAILURE(                                                \
        isolate,                                                             \
        JSTemporal##T ::METHOD(isolate, obj, args.atOrUndefined(isolate, 1), \
                               args.atOrUndefined(isolate, 2),               \
                               args.atOrUndefined(isolate, 3)));             \
  }

#define TEMPORAL_METHOD2(T, METHOD)                                     \
  BUILTIN(Temporal##T##METHOD) {                                        \
    HandleScope scope(isolate);                                         \
    RETURN_RESULT_OR_FAILURE(                                           \
        isolate,                                                        \
        JSTemporal##T ::METHOD(isolate, args.atOrUndefined(isolate, 1), \
                               args.atOrUndefined(isolate, 2)));        \
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

#define TEMPORAL_GET_SMI(T, METHOD, field)                        \
  BUILTIN(Temporal##T##Prototype##METHOD) {                       \
    HandleScope scope(isolate);                                   \
    const char* method = "get Temporal." #T ".prototype." #field; \
    CHECK_RECEIVER(JSTemporal##T, obj, method);                   \
    return Smi::FromInt(obj->field());                            \
  }

#define TEMPORAL_METHOD1(T, METHOD)                                       \
  BUILTIN(Temporal##T##METHOD) {                                          \
    HandleScope scope(isolate);                                           \
    RETURN_RESULT_OR_FAILURE(                                             \
        isolate,                                                          \
        JSTemporal##T ::METHOD(isolate, args.atOrUndefined(isolate, 1))); \
  }

#define TEMPORAL_GET(T, METHOD, field)                            \
  BUILTIN(Temporal##T##Prototype##METHOD) {                       \
    HandleScope scope(isolate);                                   \
    const char* method = "get Temporal." #T ".prototype." #field; \
    CHECK_RECEIVER(JSTemporal##T, obj, method);                   \
    return obj->field();                                          \
  }

#define TEMPORAL_GET_NUMBER_AFTER_DIVID(T, M, field, scale, name)         \
  BUILTIN(Temporal##T##Prototype##M) {                                    \
    HandleScope scope(isolate);                                           \
    const char* method = "get Temporal." #T ".prototype." #name;          \
    CHECK_RECEIVER(JSTemporal##T, handle, method);                        \
    Handle<BigInt> value;                                                 \
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(                                   \
        isolate, value,                                                   \
        BigInt::Divide(isolate, Handle<BigInt>(handle->field(), isolate), \
                       BigInt::FromUint64(isolate, scale)));              \
    Handle<Object> number = BigInt::ToNumber(isolate, value);             \
    DCHECK(std::isfinite(number->Number()));                              \
    return *number;                                                       \
  }

#define TEMPORAL_GET_BIGINT_AFTER_DIVID(T, M, field, scale, name)         \
  BUILTIN(Temporal##T##Prototype##M) {                                    \
    HandleScope scope(isolate);                                           \
    const char* method = "get Temporal." #T ".prototype." #name;          \
    CHECK_RECEIVER(JSTemporal##T, handle, method);                        \
    Handle<BigInt> value;                                                 \
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(                                   \
        isolate, value,                                                   \
        BigInt::Divide(isolate, Handle<BigInt>(handle->field(), isolate), \
                       BigInt::FromUint64(isolate, scale)));              \
    return *value;                                                        \
  }

#define TEMPORAL_GET_BY_FORWARD_CALENDAR(T, METHOD, name)                     \
  BUILTIN(Temporal##T##Prototype##METHOD) {                                   \
    HandleScope scope(isolate);                                               \
    const char* method = "get Temporal." #T ".prototype." #name;              \
    CHECK_RECEIVER(JSTemporal##T, temporal_date, method);                     \
    Handle<JSReceiver> calendar = handle(temporal_date->calendar(), isolate); \
    RETURN_RESULT_OR_FAILURE(isolate, temporal::Calendar##METHOD(             \
                                          isolate, calendar, temporal_date)); \
  }

#define TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(T, METHOD, name)              \
  BUILTIN(Temporal##T##Prototype##METHOD) {                                  \
    HandleScope scope(isolate);                                              \
    const char* method = "get Temporal." #T ".prototype." #name;             \
    /* 2. Perform ? RequireInternalSlot(temporalDate, [[InitializedTemporal  \
     * #T]]). */                                                             \
    CHECK_RECEIVER(JSTemporal##T, date_like, method);                        \
    /* 3. Let calendar be temporalDate.[[Calendar]]. */                      \
    Handle<JSReceiver> calendar = handle(date_like->calendar(), isolate);    \
    /* 2. Return ? Invoke(calendar, "name", « dateLike »).  */             \
    RETURN_RESULT_OR_FAILURE(                                                \
        isolate, temporal::InvokeCalendarMethod(                             \
                     isolate, calendar, isolate->factory()->name##_string(), \
                     date_like));                                            \
  }

// Now
TEMPORAL_NOW0(TimeZone)
TEMPORAL_NOW0(Instant)
TEMPORAL_NOW2(PlainDateTime)
TEMPORAL_NOW_ISO1(PlainDateTime)
TEMPORAL_NOW2(PlainDate)
TEMPORAL_NOW_ISO1(PlainDate)

// There is NO Temporal.now.plainTime
// See https://github.com/tc39/proposal-temporal/issues/1540
TEMPORAL_NOW_ISO1(PlainTime)
TEMPORAL_NOW2(ZonedDateTime)
TEMPORAL_NOW_ISO1(ZonedDateTime)

// PlainDate
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
TEMPORAL_METHOD2(PlainDate, From)
TEMPORAL_GET(PlainDate, Calendar, calendar)
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainDate, Year, year)
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainDate, Month, month)
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainDate, MonthCode, monthCode)
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainDate, Day, day)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainDate, DayOfWeek, dayOfWeek)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainDate, DayOfYear, dayOfYear)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainDate, WeekOfYear, weekOfYear)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainDate, DaysInWeek, daysInWeek)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainDate, DaysInMonth, daysInMonth)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainDate, DaysInYear, daysInYear)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainDate, MonthsInYear, monthsInYear)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainDate, InLeapYear, inLeapYear)
TEMPORAL_PROTOTYPE_METHOD1(PlainDate, WithCalendar, withCalendar)
TEMPORAL_PROTOTYPE_METHOD0(PlainDate, GetISOFields, getISOFields)
TEMPORAL_VALUE_OF(PlainDate)

// PlainTime
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
TEMPORAL_GET(PlainTime, Calendar, calendar)
TEMPORAL_GET_SMI(PlainTime, Hour, iso_hour)
TEMPORAL_GET_SMI(PlainTime, Minute, iso_minute)
TEMPORAL_GET_SMI(PlainTime, Second, iso_second)
TEMPORAL_GET_SMI(PlainTime, Millisecond, iso_millisecond)
TEMPORAL_GET_SMI(PlainTime, Microsecond, iso_microsecond)
TEMPORAL_GET_SMI(PlainTime, Nanosecond, iso_nanosecond)
TEMPORAL_METHOD2(PlainTime, From)
TEMPORAL_PROTOTYPE_METHOD0(PlainTime, GetISOFields, getISOFields)
TEMPORAL_VALUE_OF(PlainTime)

// PlainDateTime
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
TEMPORAL_GET(PlainDateTime, Calendar, calendar)
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainDateTime, Year, year)
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainDateTime, Month, month)
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainDateTime, MonthCode, monthCode)
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainDateTime, Day, day)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainDateTime, DayOfWeek, dayOfWeek)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainDateTime, DayOfYear, dayOfYear)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainDateTime, WeekOfYear, weekOfYear)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainDateTime, DaysInWeek, daysInWeek)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainDateTime, DaysInMonth, daysInMonth)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainDateTime, DaysInYear, daysInYear)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainDateTime, MonthsInYear,
                                       monthsInYear)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainDateTime, InLeapYear, inLeapYear)
TEMPORAL_PROTOTYPE_METHOD1(PlainDateTime, WithCalendar, withCalendar)
TEMPORAL_GET_SMI(PlainDateTime, Hour, iso_hour)
TEMPORAL_GET_SMI(PlainDateTime, Minute, iso_minute)
TEMPORAL_GET_SMI(PlainDateTime, Second, iso_second)
TEMPORAL_GET_SMI(PlainDateTime, Millisecond, iso_millisecond)
TEMPORAL_GET_SMI(PlainDateTime, Microsecond, iso_microsecond)
TEMPORAL_GET_SMI(PlainDateTime, Nanosecond, iso_nanosecond)
TEMPORAL_PROTOTYPE_METHOD0(PlainDateTime, GetISOFields, getISOFields)
TEMPORAL_VALUE_OF(PlainDateTime)

// PlainYearMonth
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
TEMPORAL_GET(PlainYearMonth, Calendar, calendar)
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainYearMonth, Year, year)
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainYearMonth, Month, month)
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainYearMonth, MonthCode, monthCode)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainYearMonth, DaysInYear, daysInYear)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainYearMonth, DaysInMonth, daysInMonth)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainYearMonth, MonthsInYear,
                                       monthsInYear)
TEMPORAL_GET_BY_INVOKE_CALENDAR_METHOD(PlainYearMonth, InLeapYear, inLeapYear)
TEMPORAL_PROTOTYPE_METHOD0(PlainYearMonth, GetISOFields, getISOFields)
TEMPORAL_VALUE_OF(PlainYearMonth)

// PlainMonthDay
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
TEMPORAL_GET(PlainMonthDay, Calendar, calendar)
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainMonthDay, MonthCode, monthCode)
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainMonthDay, Day, day)
TEMPORAL_PROTOTYPE_METHOD0(PlainMonthDay, GetISOFields, getISOFields)
TEMPORAL_VALUE_OF(PlainMonthDay)

// ZonedDateTime

#define TEMPORAL_ZONED_DATE_TIME_GET_PREPARE(M)                               \
  HandleScope scope(isolate);                                                 \
  const char* method = "get Temporal.ZonedDateTime.prototype." #M;            \
  /* 1. Let zonedDateTime be the this value. */                               \
  /* 2. Perform ? RequireInternalSlot(zonedDateTime, */                       \
  /* [[InitializedTemporalZonedDateTime]]). */                                \
  CHECK_RECEIVER(JSTemporalZonedDateTime, zoned_date_time, method);           \
  /* 3. Let timeZone be zonedDateTime.[[TimeZone]]. */                        \
  Handle<JSReceiver> time_zone =                                              \
      handle(zoned_date_time->time_zone(), isolate);                          \
  /* 4. Let instant be ?                                   */                 \
  /* CreateTemporalInstant(zonedDateTime.[[Nanoseconds]]). */                 \
  Handle<JSTemporalInstant> instant;                                          \
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(                                         \
      isolate, instant,                                                       \
      temporal::CreateTemporalInstant(                                        \
          isolate, Handle<BigInt>(zoned_date_time->nanoseconds(), isolate))); \
  /* 5. Let calendar be zonedDateTime.[[Calendar]]. */                        \
  Handle<JSReceiver> calendar = handle(zoned_date_time->calendar(), isolate); \
  /* 6. Let temporalDateTime be ?                 */                          \
  /* BuiltinTimeZoneGetPlainDateTimeFor(timeZone, */                          \
  /* instant, calendar). */                                                   \
  Handle<JSTemporalPlainDateTime> temporal_date_time;                         \
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(                                         \
      isolate, temporal_date_time,                                            \
      temporal::BuiltinTimeZoneGetPlainDateTimeFor(                           \
          isolate, time_zone, instant, calendar, method));

#define TEMPORAL_ZONED_DATE_TIME_GET_INT_BY_FORWARD_TIME_ZONE(M, field) \
  BUILTIN(TemporalZonedDateTimePrototype##M) {                          \
    TEMPORAL_ZONED_DATE_TIME_GET_PREPARE(M)                             \
    /* 7. Return 𝔽(temporalDateTime.[[ #field ]]). */                \
    return Smi::FromInt(temporal_date_time->field());                   \
  }

BUILTIN(TemporalZonedDateTimeConstructor) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSTemporalZonedDateTime::Constructor(
                   isolate, args.target(), args.new_target(),
                   args.atOrUndefined(isolate, 1),    // epoch_nanoseconds
                   args.atOrUndefined(isolate, 2),    // time_zone_like
                   args.atOrUndefined(isolate, 3)));  // calendar_like
}
TEMPORAL_GET(ZonedDateTime, Calendar, calendar)
TEMPORAL_GET(ZonedDateTime, TimeZone, time_zone)
TEMPORAL_GET(ZonedDateTime, EpochNanoseconds, nanoseconds)
TEMPORAL_GET_NUMBER_AFTER_DIVID(ZonedDateTime, EpochSeconds, nanoseconds,
                                1000000000, epochSeconds)
TEMPORAL_GET_NUMBER_AFTER_DIVID(ZonedDateTime, EpochMilliseconds, nanoseconds,
                                1000000, epochMilliseconds)
TEMPORAL_GET_BIGINT_AFTER_DIVID(ZonedDateTime, EpochMicroseconds, nanoseconds,
                                1000, epochMicroseconds)
TEMPORAL_ZONED_DATE_TIME_GET_INT_BY_FORWARD_TIME_ZONE(Hour, iso_hour)
TEMPORAL_ZONED_DATE_TIME_GET_INT_BY_FORWARD_TIME_ZONE(Minute, iso_minute)
TEMPORAL_ZONED_DATE_TIME_GET_INT_BY_FORWARD_TIME_ZONE(Second, iso_second)
TEMPORAL_ZONED_DATE_TIME_GET_INT_BY_FORWARD_TIME_ZONE(Millisecond,
                                                      iso_millisecond)
TEMPORAL_ZONED_DATE_TIME_GET_INT_BY_FORWARD_TIME_ZONE(Microsecond,
                                                      iso_microsecond)
TEMPORAL_ZONED_DATE_TIME_GET_INT_BY_FORWARD_TIME_ZONE(Nanosecond,
                                                      iso_nanosecond)
TEMPORAL_PROTOTYPE_METHOD1(ZonedDateTime, WithCalendar, withCalendar)
TEMPORAL_PROTOTYPE_METHOD1(ZonedDateTime, WithTimeZone, withTimeZone)
TEMPORAL_PROTOTYPE_METHOD0(ZonedDateTime, ToPlainYearMonth, toPlainYearMonth)
TEMPORAL_PROTOTYPE_METHOD0(ZonedDateTime, ToPlainMonthDay, toPlainMonthDay)
TEMPORAL_PROTOTYPE_METHOD0(ZonedDateTime, GetISOFields, getISOFields)
TEMPORAL_VALUE_OF(ZonedDateTime)

// Duration
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
TEMPORAL_GET(Duration, Years, years)
TEMPORAL_GET(Duration, Months, months)
TEMPORAL_GET(Duration, Weeks, weeks)
TEMPORAL_GET(Duration, Days, days)
TEMPORAL_GET(Duration, Hours, hours)
TEMPORAL_GET(Duration, Minutes, minutes)
TEMPORAL_GET(Duration, Seconds, seconds)
TEMPORAL_GET(Duration, Milliseconds, milliseconds)
TEMPORAL_GET(Duration, Microseconds, microseconds)
TEMPORAL_GET(Duration, Nanoseconds, nanoseconds)
TEMPORAL_PROTOTYPE_METHOD0(Duration, Sign, sign)
TEMPORAL_PROTOTYPE_METHOD0(Duration, Blank, blank)
TEMPORAL_PROTOTYPE_METHOD0(Duration, Negated, negated)
TEMPORAL_PROTOTYPE_METHOD0(Duration, Abs, abs)
TEMPORAL_VALUE_OF(Duration)

// Instant
TEMPORAL_CONSTRUCTOR1(Instant)
TEMPORAL_METHOD1(Instant, FromEpochSeconds)
TEMPORAL_METHOD1(Instant, FromEpochMilliseconds)
TEMPORAL_METHOD1(Instant, FromEpochMicroseconds)
TEMPORAL_METHOD1(Instant, FromEpochNanoseconds)
TEMPORAL_METHOD1(Instant, From)
TEMPORAL_VALUE_OF(Instant)
TEMPORAL_GET(Instant, EpochNanoseconds, nanoseconds)
TEMPORAL_GET_NUMBER_AFTER_DIVID(Instant, EpochSeconds, nanoseconds, 1000000000,
                                epochSeconds)
TEMPORAL_GET_NUMBER_AFTER_DIVID(Instant, EpochMilliseconds, nanoseconds,
                                1000000, epochMilliseconds)
TEMPORAL_GET_BIGINT_AFTER_DIVID(Instant, EpochMicroseconds, nanoseconds, 1000,
                                epochMicroseconds)

// Calendar
TEMPORAL_CONSTRUCTOR1(Calendar)
TEMPORAL_ID_BY_TO_STRING(Calendar)
TEMPORAL_PROTOTYPE_METHOD2(Calendar, DateFromFields, dateFromFields)
TEMPORAL_PROTOTYPE_METHOD1(Calendar, DaysInMonth, daysInMonth)
TEMPORAL_PROTOTYPE_METHOD1(Calendar, DaysInWeek, daysInWeek)
TEMPORAL_PROTOTYPE_METHOD1(Calendar, DaysInYear, daysInYear)
TEMPORAL_PROTOTYPE_METHOD1(Calendar, DayOfWeek, dayOfWeek)
TEMPORAL_PROTOTYPE_METHOD1(Calendar, DayOfYear, dayOfYear)
TEMPORAL_PROTOTYPE_METHOD1(Calendar, InLeapYear, inLeapYear)
TEMPORAL_PROTOTYPE_METHOD2(Calendar, MergeFields, mergeFields)
TEMPORAL_PROTOTYPE_METHOD1(Calendar, MonthsInYear, monthsInYear)
TEMPORAL_PROTOTYPE_METHOD1(Calendar, Year, year)
TEMPORAL_TO_STRING(Calendar)
// #sec-temporal.calendar.from
BUILTIN(TemporalCalendarFrom) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(isolate, temporal::ToTemporalCalendar(
                                        isolate, args.atOrUndefined(isolate, 1),
                                        "Temporal.Calendar.from"));
}

// TimeZone
TEMPORAL_CONSTRUCTOR1(TimeZone)
TEMPORAL_ID_BY_TO_STRING(TimeZone)
TEMPORAL_TO_STRING(TimeZone)
// #sec-temporal.timezone.from
BUILTIN(TemporalTimeZoneFrom) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(isolate, temporal::ToTemporalTimeZone(
                                        isolate, args.atOrUndefined(isolate, 1),
                                        "Temporal.TimeZone.from"));
}

#ifdef V8_INTL_SUPPORT
// get Temporal.*.prototype.era/eraYear
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainDate, Era, era)
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainDate, EraYear, eraYear)
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainDateTime, Era, era)
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainDateTime, EraYear, eraYear)
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainYearMonth, Era, era)
TEMPORAL_GET_BY_FORWARD_CALENDAR(PlainYearMonth, EraYear, eraYear)
#endif  // V8_INTL_SUPPORT
}  // namespace internal
}  // namespace v8
