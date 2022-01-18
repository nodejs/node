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
  BUILTIN(id) {                 \
    HandleScope scope(isolate); \
    UNIMPLEMENTED();            \
  }

/* Temporal #sec-temporal.now.timezone */
TO_BE_IMPLEMENTED(TemporalNowTimeZone)
/* Temporal #sec-temporal.now.instant */
TO_BE_IMPLEMENTED(TemporalNowInstant)
/* Temporal #sec-temporal.now.plaindatetime */
TO_BE_IMPLEMENTED(TemporalNowPlainDateTime)
/* Temporal #sec-temporal.now.plaindatetimeiso */
TO_BE_IMPLEMENTED(TemporalNowPlainDateTimeISO)
/* Temporal #sec-temporal.now.zoneddatetime */
TO_BE_IMPLEMENTED(TemporalNowZonedDateTime)
/* Temporal #sec-temporal.now.zoneddatetimeiso */
TO_BE_IMPLEMENTED(TemporalNowZonedDateTimeISO)
/* Temporal #sec-temporal.now.plaindate */
TO_BE_IMPLEMENTED(TemporalNowPlainDate)
/* Temporal #sec-temporal.now.plaindateiso */
TO_BE_IMPLEMENTED(TemporalNowPlainDateISO)
/* There are no Temporal.now.plainTime */
/* See https://github.com/tc39/proposal-temporal/issues/1540 */
/* Temporal #sec-temporal.now.plaintimeiso */
TO_BE_IMPLEMENTED(TemporalNowPlainTimeISO)

/* Temporal.PlaneDate */
/* Temporal #sec-temporal.plaindate */
TO_BE_IMPLEMENTED(TemporalPlainDateConstructor)
/* Temporal #sec-temporal.plaindate.from */
TO_BE_IMPLEMENTED(TemporalPlainDateFrom)
/* Temporal #sec-temporal.plaindate.compare */
TO_BE_IMPLEMENTED(TemporalPlainDateCompare)
/* Temporal #sec-get-temporal.plaindate.prototype.calendar */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeCalendar)
/* Temporal #sec-get-temporal.plaindate.prototype.year */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeYear)
/* Temporal #sec-get-temporal.plaindate.prototype.month */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeMonth)
/* Temporal #sec-get-temporal.plaindate.prototype.monthcode */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeMonthCode)
/* Temporal #sec-get-temporal.plaindate.prototype.day */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeDay)
/* Temporal #sec-get-temporal.plaindate.prototype.dayofweek */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeDayOfWeek)
/* Temporal #sec-get-temporal.plaindate.prototype.dayofyear */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeDayOfYear)
/* Temporal #sec-get-temporal.plaindate.prototype.weekofyear */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeWeekOfYear)
/* Temporal #sec-get-temporal.plaindate.prototype.daysinweek */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeDaysInWeek)
/* Temporal #sec-get-temporal.plaindate.prototype.daysinmonth */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeDaysInMonth)
/* Temporal #sec-get-temporal.plaindate.prototype.daysinyear */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeDaysInYear)
/* Temporal #sec-get-temporal.plaindate.prototype.monthsinyear */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeMonthsInYear)
/* Temporal #sec-get-temporal.plaindate.prototype.inleapyear */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeInLeapYear)
/* Temporal #sec-temporal.plaindate.prototype.toplainyearmonth */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeToPlainYearMonth)
/* Temporal #sec-temporal.plaindate.prototype.toplainmonthday */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeToPlainMonthDay)
/* Temporal #sec-temporal.plaindate.prototype.getisofields */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeGetISOFields)
/* Temporal #sec-temporal.plaindate.prototype.add */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeAdd)
/* Temporal #sec-temporal.plaindate.prototype.substract */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeSubtract)
/* Temporal #sec-temporal.plaindate.prototype.with */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeWith)
/* Temporal #sec-temporal.plaindate.prototype.withcalendar */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeWithCalendar)
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
/* Temporal #sec-temporal.plaindate.prototype.valueof */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeValueOf)

/* Temporal.PlaneTime */
/* Temporal #sec-temporal.plaintime */
TO_BE_IMPLEMENTED(TemporalPlainTimeConstructor)
/* Temporal #sec-temporal.plaintime.from */
TO_BE_IMPLEMENTED(TemporalPlainTimeFrom)
/* Temporal #sec-temporal.plaintime.compare */
TO_BE_IMPLEMENTED(TemporalPlainTimeCompare)
/* Temporal #sec-get-temporal.plaintime.prototype.calendar */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeCalendar)
/* Temporal #sec-get-temporal.plaintime.prototype.hour */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeHour)
/* Temporal #sec-get-temporal.plaintime.prototype.minute */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeMinute)
/* Temporal #sec-get-temporal.plaintime.prototype.second */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeSecond)
/* Temporal #sec-get-temporal.plaintime.prototype.millisecond */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeMillisecond)
/* Temporal #sec-get-temporal.plaintime.prototype.microsecond */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeMicrosecond)
/* Temporal #sec-get-temporal.plaintime.prototype.nanoseond */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeNanosecond)
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
/* Temporal #sec-temporal.plaintime.prototype.getisofields */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeGetISOFields)
/* Temporal #sec-temporal.plaintime.prototype.tostring */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeToString)
/* Temporal #sec-temporal.plaindtimeprototype.tojson */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeToJSON)
/* Temporal #sec-temporal.plaintime.prototype.valueof */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeValueOf)

/* Temporal.PlaneDateTime */
/* Temporal #sec-temporal.plaindatetime */
TO_BE_IMPLEMENTED(TemporalPlainDateTimeConstructor)
/* Temporal #sec-temporal.plaindatetime.from */
TO_BE_IMPLEMENTED(TemporalPlainDateTimeFrom)
/* Temporal #sec-temporal.plaindatetime.compare */
TO_BE_IMPLEMENTED(TemporalPlainDateTimeCompare)
/* Temporal #sec-get-temporal.plaindatetime.prototype.calendar */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeCalendar)
/* Temporal #sec-get-temporal.plaindatetime.prototype.year */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeYear)
/* Temporal #sec-get-temporal.plaindatetime.prototype.month */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeMonth)
/* Temporal #sec-get-temporal.plaindatetime.prototype.monthcode */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeMonthCode)
/* Temporal #sec-get-temporal.plaindatetime.prototype.day */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeDay)
/* Temporal #sec-get-temporal.plaindatetime.prototype.hour */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeHour)
/* Temporal #sec-get-temporal.plaindatetime.prototype.minute */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeMinute)
/* Temporal #sec-get-temporal.plaindatetime.prototype.second */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeSecond)
/* Temporal #sec-get-temporal.plaindatetime.prototype.millisecond */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeMillisecond)
/* Temporal #sec-get-temporal.plaindatetime.prototype.microsecond */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeMicrosecond)
/* Temporal #sec-get-temporal.plaindatetime.prototype.nanosecond */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeNanosecond)
/* Temporal #sec-get-temporal.plaindatetime.prototype.dayofweek */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeDayOfWeek)
/* Temporal #sec-get-temporal.plaindatetime.prototype.dayofyear */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeDayOfYear)
/* Temporal #sec-get-temporal.plaindatetime.prototype.weekofyear */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeWeekOfYear)
/* Temporal #sec-get-temporal.plaindatetime.prototype.daysinweek */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeDaysInWeek)
/* Temporal #sec-get-temporal.plaindatetime.prototype.daysinmonth */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeDaysInMonth)
/* Temporal #sec-get-temporal.plaindatetime.prototype.daysinyear */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeDaysInYear)
/* Temporal #sec-get-temporal.plaindatetime.prototype.monthsinyear */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeMonthsInYear)
/* Temporal #sec-get-temporal.plaindatetime.prototype.inleapyear */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeInLeapYear)
/* Temporal #sec-temporal.plaindatetime.prototype.with */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeWith)
/* Temporal #sec-temporal.plaindatetime.prototype.withplainTime */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeWithPlainTime)
/* Temporal #sec-temporal.plaindatetime.prototype.withplainDate */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeWithPlainDate)
/* Temporal #sec-temporal.plaindatetime.prototype.withcalendar */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeWithCalendar)
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
/* Temporal #sec-temporal.plaindatetime.prototype.valueof */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeValueOf)
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
/* Temporal #sec-temporal.plaindatetime.prototype.getisofields */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeGetISOFields)

/* Temporal.ZonedDateTime */
/* Temporal #sec-temporal.zoneddatetime */
TO_BE_IMPLEMENTED(TemporalZonedDateTimeConstructor)
/* Temporal #sec-temporal.zoneddatetime.from */
TO_BE_IMPLEMENTED(TemporalZonedDateTimeFrom)
/* Temporal #sec-temporal.zoneddatetime.compare */
TO_BE_IMPLEMENTED(TemporalZonedDateTimeCompare)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.calendar */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeCalendar)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.timezone */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeTimeZone)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.year */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeYear)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.month */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeMonth)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.monthcode */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeMonthCode)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.day */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeDay)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.hour */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeHour)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.minute */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeMinute)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.second */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeSecond)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.millisecond */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeMillisecond)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.microsecond */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeMicrosecond)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.nanosecond */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeNanosecond)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.epochsecond */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeEpochSeconds)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.epochmilliseconds */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeEpochMilliseconds)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.epochmicroseconds */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeEpochMicroseconds)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.epochnanoseconds */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeEpochNanoseconds)
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
/* Temporal #sec-temporal.zoneddatetime.prototype.withtimezone */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeWithTimeZone)
/* Temporal #sec-temporal.zoneddatetime.prototype.withcalendar */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeWithCalendar)
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
/* Temporal #sec-temporal.zoneddatetime.prototype.valueof */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeValueOf)
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
/* Temporal #sec-temporal.zoneddatetime.prototype.toplainyearmonth */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToPlainYearMonth)
/* Temporal #sec-temporal.zoneddatetime.prototype.toplainmonthday */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToPlainMonthDay)
/* Temporal #sec-temporal.zoneddatetime.prototype.getisofields */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeGetISOFields)

/* Temporal.Duration */
/* Temporal #sec-temporal.duration */
TO_BE_IMPLEMENTED(TemporalDurationConstructor)
/* Temporal #sec-temporal.duration.from */
TO_BE_IMPLEMENTED(TemporalDurationFrom)
/* Temporal #sec-temporal.duration.compare */
TO_BE_IMPLEMENTED(TemporalDurationCompare)
/* Temporal #sec-get-temporal.duration.prototype.years */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeYears)
/* Temporal #sec-get-temporal.duration.prototype.months */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeMonths)
/* Temporal #sec-get-temporal.duration.prototype.weeks */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeWeeks)
/* Temporal #sec-get-temporal.duration.prototype.days */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeDays)
/* Temporal #sec-get-temporal.duration.prototype.hours */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeHours)
/* Temporal #sec-get-temporal.duration.prototype.minutes */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeMinutes)
/* Temporal #sec-get-temporal.duration.prototype.seconds */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeSeconds)
/* Temporal #sec-get-temporal.duration.prototype.milliseconds */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeMilliseconds)
/* Temporal #sec-get-temporal.duration.prototype.microseconds */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeMicroseconds)
/* Temporal #sec-get-temporal.duration.prototype.nanoseconds */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeNanoseconds)
/* Temporal #sec-get-temporal.duration.prototype.sign */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeSign)
/* Temporal #sec-get-temporal.duration.prototype.blank */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeBlank)
/* Temporal #sec-temporal.duration.prototype.with */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeWith)
/* Temporal #sec-temporal.duration.prototype.negated */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeNegated)
/* Temporal #sec-temporal.duration.prototype.abs */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeAbs)
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
/* Temporal #sec-temporal.duration.prototype.valueof */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeValueOf)

/* Temporal.Instant */
/* Temporal #sec-temporal.instant */
TO_BE_IMPLEMENTED(TemporalInstantConstructor)
/* Temporal #sec-temporal.instant.from */
TO_BE_IMPLEMENTED(TemporalInstantFrom)
/* Temporal #sec-temporal.instant.fromepochseconds */
TO_BE_IMPLEMENTED(TemporalInstantFromEpochSeconds)
/* Temporal #sec-temporal.instant.fromepochmilliseconds */
TO_BE_IMPLEMENTED(TemporalInstantFromEpochMilliseconds)
/* Temporal #sec-temporal.instant.fromepochmicroseconds */
TO_BE_IMPLEMENTED(TemporalInstantFromEpochMicroseconds)
/* Temporal #sec-temporal.instant.fromepochnanoseconds */
TO_BE_IMPLEMENTED(TemporalInstantFromEpochNanoseconds)
/* Temporal #sec-temporal.instant.compare */
TO_BE_IMPLEMENTED(TemporalInstantCompare)
/* Temporal #sec-get-temporal.instant.prototype.epochseconds */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeEpochSeconds)
/* Temporal #sec-get-temporal.instant.prototype.epochmilliseconds */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeEpochMilliseconds)
/* Temporal #sec-get-temporal.instant.prototype.epochmicroseconds */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeEpochMicroseconds)
/* Temporal #sec-get-temporal.instant.prototype.epochnanoseconds */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeEpochNanoseconds)
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
/* Temporal #sec-temporal.instant.prototype.valueof */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeValueOf)
/* Temporal #sec-temporal.instant.prototype.tozoneddatetime */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeToZonedDateTime)
/* Temporal #sec-temporal.instant.prototype.tozoneddatetimeiso */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeToZonedDateTimeISO)

/* Temporal.PlainYearMonth */
/* Temporal #sec-temporal.plainyearmonth */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthConstructor)
/* Temporal #sec-temporal.plainyearmonth.from */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthFrom)
/* Temporal #sec-temporal.plainyearmonth.compare */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthCompare)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.calendar */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeCalendar)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.year */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeYear)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.month */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeMonth)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.monthcode */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeMonthCode)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.daysinyear */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeDaysInYear)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.daysinmonth */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeDaysInMonth)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.monthsinyear */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeMonthsInYear)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.inleapyear */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeInLeapYear)
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
/* Temporal #sec-temporal.plainyearmonth.prototype.valueof */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeValueOf)
/* Temporal #sec-temporal.plainyearmonth.prototype.toplaindate */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeToPlainDate)
/* Temporal #sec-temporal.plainyearmonth.prototype.getisofields */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeGetISOFields)

/* Temporal.PlainMonthDay */
/* Temporal #sec-temporal.plainmonthday */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayConstructor)
/* Temporal #sec-temporal.plainmonthday.from */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayFrom)
/* There are no compare for PlainMonthDay */
/* See https://github.com/tc39/proposal-temporal/issues/1547 */
/* Temporal #sec-get-temporal.plainmonthday.prototype.calendar */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeCalendar)
/* Temporal #sec-get-temporal.plainmonthday.prototype.monthcode */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeMonthCode)
/* Temporal #sec-get-temporal.plainmonthday.prototype.day */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeDay)
/* Temporal #sec-temporal.plainmonthday.prototype.with */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeWith)
/* Temporal #sec-temporal.plainmonthday.prototype.equals */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeEquals)
/* Temporal #sec-temporal.plainmonthday.prototype.tostring */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeToString)
/* Temporal #sec-temporal.plainmonthday.tojson */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeToJSON)
/* Temporal #sec-temporal.plainmonthday.prototype.valueof */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeValueOf)
/* Temporal #sec-temporal.plainmonthday.prototype.toplaindate */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeToPlainDate)
/* Temporal #sec-temporal.plainmonthday.prototype.getisofields */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeGetISOFields)

/* Temporal.TimeZone */
/* Temporal #sec-temporal.timezone */
TO_BE_IMPLEMENTED(TemporalTimeZoneConstructor)
/* Temporal #sec-temporal.timezone.from */
TO_BE_IMPLEMENTED(TemporalTimeZoneFrom)
/* Temporal #sec-get-temporal.timezone.prototype.id */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeId)
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
/* Temporal #sec-temporal.timezone.prototype.tostring */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeToString)
/* Temporal #sec-temporal.timezone.prototype.tojson */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeToJSON)

/* Temporal.Calendar */
/* Temporal #sec-temporal.calendar */
TO_BE_IMPLEMENTED(TemporalCalendarConstructor)
/* Temporal #sec-temporal.calendar.from */
TO_BE_IMPLEMENTED(TemporalCalendarFrom)
/* Temporal #sec-get-temporal.calendar.prototype.id */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeId)
/* Temporal #sec-temporal.calendar.prototype.datefromfields */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDateFromFields)
/* Temporal #sec-temporal.calendar.prototype.yearmonthfromfields */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeYearMonthFromFields)
/* Temporal #sec-temporal.calendar.prototype.monthdayfromfields */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeMonthDayFromFields)
/* Temporal #sec-temporal.calendar.prototype.dateadd */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDateAdd)
/* Temporal #sec-temporal.calendar.prototype.dateuntil */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDateUntil)
/* Temporal #sec-temporal.calendar.prototype.year */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeYear)
/* Temporal #sec-temporal.calendar.prototype.month */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeMonth)
/* Temporal #sec-temporal.calendar.prototype.monthcode */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeMonthCode)
/* Temporal #sec-temporal.calendar.prototype.day */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDay)
/* Temporal #sec-temporal.calendar.prototype.dayofweek */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDayOfWeek)
/* Temporal #sec-temporal.calendar.prototype.dayofyear */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDayOfYear)
/* Temporal #sec-temporal.calendar.prototype.weekofyear */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeWeekOfYear)
/* Temporal #sec-temporal.calendar.prototype.daysinweek */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDaysInWeek)
/* Temporal #sec-temporal.calendar.prototype.daysinmonth */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDaysInMonth)
/* Temporal #sec-temporal.calendar.prototype.daysinyear */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDaysInYear)
/* Temporal #sec-temporal.calendar.prototype.monthsinyear */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeMonthsInYear)
/* Temporal #sec-temporal.calendar.prototype.inleapyear */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeInLeapYear)
/* Temporal #sec-temporal.calendar.prototype.fields */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeFields)
/* Temporal #sec-temporal.calendar.prototype.mergefields */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeMergeFields)
/* Temporal #sec-temporal.calendar.prototype.tostring */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeToString)
/* Temporal #sec-temporal.calendar.prototype.tojson */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeToJSON)

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
/* Temporal #sec-get-temporal.plaindate.prototype.era */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeEra)
/* Temporal #sec-get-temporal.plaindate.prototype.erayear */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeEraYear)
/* Temporal #sec-temporal.plaindate.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeToLocaleString)
/* Temporal #sec-get-temporal.plaindatetime.prototype.era */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeEra)
/* Temporal #sec-get-temporal.plaindatetime.prototype.erayear */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeEraYear)
/* Temporal #sec-temporal.plaindatetime.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeToLocaleString)
/* Temporal #sec-temporal.plainmonthday.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeToLocaleString)
/* Temporal #sec-temporal.plaintime.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeToLocaleString)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.era */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeEra)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.erayear */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeEraYear)
/* Temporal #sec-temporal.plainyearmonth.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeToLocaleString)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.era */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeEra)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.erayear */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeEraYear)
/* Temporal #sec-temporal.zoneddatetime.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToLocaleString)
#endif  // V8_INTL_SUPPORT

}  // namespace internal
}  // namespace v8
