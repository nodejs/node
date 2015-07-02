// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var $createDate;

// -------------------------------------------------------------------

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalDate = global.Date;
var InternalArray = utils.InternalArray;

var IsFinite;
var MathAbs;
var MathFloor;

utils.Import(function(from) {
  IsFinite = from.IsFinite;
  MathAbs = from.MathAbs;
  MathFloor = from.MathFloor;
});

// -------------------------------------------------------------------

// This file contains date support implemented in JavaScript.

var timezone_cache_time = NAN;
var timezone_cache_timezone;

function LocalTimezone(t) {
  if (NUMBER_IS_NAN(t)) return "";
  CheckDateCacheCurrent();
  if (t == timezone_cache_time) {
    return timezone_cache_timezone;
  }
  var timezone = %DateLocalTimezone(t);
  timezone_cache_time = t;
  timezone_cache_timezone = timezone;
  return timezone;
}


function UTC(time) {
  if (NUMBER_IS_NAN(time)) return time;
  // local_time_offset is needed before the call to DaylightSavingsOffset,
  // so it may be uninitialized.
  return %DateToUTC(time);
}


// ECMA 262 - 15.9.1.11
function MakeTime(hour, min, sec, ms) {
  if (!IsFinite(hour)) return NAN;
  if (!IsFinite(min)) return NAN;
  if (!IsFinite(sec)) return NAN;
  if (!IsFinite(ms)) return NAN;
  return TO_INTEGER(hour) * msPerHour
      + TO_INTEGER(min) * msPerMinute
      + TO_INTEGER(sec) * msPerSecond
      + TO_INTEGER(ms);
}


// ECMA 262 - 15.9.1.12
function TimeInYear(year) {
  return DaysInYear(year) * msPerDay;
}


// Compute number of days given a year, month, date.
// Note that month and date can lie outside the normal range.
//   For example:
//     MakeDay(2007, -4, 20) --> MakeDay(2006, 8, 20)
//     MakeDay(2007, -33, 1) --> MakeDay(2004, 3, 1)
//     MakeDay(2007, 14, -50) --> MakeDay(2007, 8, 11)
function MakeDay(year, month, date) {
  if (!IsFinite(year) || !IsFinite(month) || !IsFinite(date)) return NAN;

  // Convert to integer and map -0 to 0.
  year = TO_INTEGER_MAP_MINUS_ZERO(year);
  month = TO_INTEGER_MAP_MINUS_ZERO(month);
  date = TO_INTEGER_MAP_MINUS_ZERO(date);

  if (year < kMinYear || year > kMaxYear ||
      month < kMinMonth || month > kMaxMonth) {
    return NAN;
  }

  // Now we rely on year and month being SMIs.
  return %DateMakeDay(year | 0, month | 0) + date - 1;
}


// ECMA 262 - 15.9.1.13
function MakeDate(day, time) {
  var time = day * msPerDay + time;
  // Some of our runtime funtions for computing UTC(time) rely on
  // times not being significantly larger than MAX_TIME_MS. If there
  // is no way that the time can be within range even after UTC
  // conversion we return NaN immediately instead of relying on
  // TimeClip to do it.
  if (MathAbs(time) > MAX_TIME_BEFORE_UTC) return NAN;
  return time;
}


// ECMA 262 - 15.9.1.14
function TimeClip(time) {
  if (!IsFinite(time)) return NAN;
  if (MathAbs(time) > MAX_TIME_MS) return NAN;
  return TO_INTEGER(time);
}


// The Date cache is used to limit the cost of parsing the same Date
// strings over and over again.
var Date_cache = {
  // Cached time value.
  time: 0,
  // String input for which the cached time is valid.
  string: null
};


function DateConstructor(year, month, date, hours, minutes, seconds, ms) {
  if (!%_IsConstructCall()) {
    // ECMA 262 - 15.9.2
    return %_CallFunction(new GlobalDate(), DateToString);
  }

  // ECMA 262 - 15.9.3
  var argc = %_ArgumentsLength();
  var value;
  if (argc == 0) {
    value = %DateCurrentTime();
    SET_UTC_DATE_VALUE(this, value);
  } else if (argc == 1) {
    if (IS_NUMBER(year)) {
      value = year;
    } else if (IS_STRING(year)) {
      // Probe the Date cache. If we already have a time value for the
      // given time, we re-use that instead of parsing the string again.
      CheckDateCacheCurrent();
      var cache = Date_cache;
      if (cache.string === year) {
        value = cache.time;
      } else {
        value = DateParse(year);
        if (!NUMBER_IS_NAN(value)) {
          cache.time = value;
          cache.string = year;
        }
      }

    } else {
      // According to ECMA 262, no hint should be given for this
      // conversion. However, ToPrimitive defaults to STRING_HINT for
      // Date objects which will lose precision when the Date
      // constructor is called with another Date object as its
      // argument. We therefore use NUMBER_HINT for the conversion,
      // which is the default for everything else than Date objects.
      // This makes us behave like KJS and SpiderMonkey.
      var time = $toPrimitive(year, NUMBER_HINT);
      value = IS_STRING(time) ? DateParse(time) : $toNumber(time);
    }
    SET_UTC_DATE_VALUE(this, value);
  } else {
    year = $toNumber(year);
    month = $toNumber(month);
    date = argc > 2 ? $toNumber(date) : 1;
    hours = argc > 3 ? $toNumber(hours) : 0;
    minutes = argc > 4 ? $toNumber(minutes) : 0;
    seconds = argc > 5 ? $toNumber(seconds) : 0;
    ms = argc > 6 ? $toNumber(ms) : 0;
    year = (!NUMBER_IS_NAN(year) &&
            0 <= TO_INTEGER(year) &&
            TO_INTEGER(year) <= 99) ? 1900 + TO_INTEGER(year) : year;
    var day = MakeDay(year, month, date);
    var time = MakeTime(hours, minutes, seconds, ms);
    value = MakeDate(day, time);
    SET_LOCAL_DATE_VALUE(this, value);
  }
}


var WeekDays = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
var Months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',
              'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];


function TwoDigitString(value) {
  return value < 10 ? "0" + value : "" + value;
}


function DateString(date) {
  CHECK_DATE(date);
  return WeekDays[LOCAL_WEEKDAY(date)] + ' '
      + Months[LOCAL_MONTH(date)] + ' '
      + TwoDigitString(LOCAL_DAY(date)) + ' '
      + LOCAL_YEAR(date);
}


var LongWeekDays = ['Sunday', 'Monday', 'Tuesday', 'Wednesday',
    'Thursday', 'Friday', 'Saturday'];
var LongMonths = ['January', 'February', 'March', 'April', 'May', 'June',
    'July', 'August', 'September', 'October', 'November', 'December'];


function LongDateString(date) {
  CHECK_DATE(date);
  return LongWeekDays[LOCAL_WEEKDAY(date)] + ', '
      + LongMonths[LOCAL_MONTH(date)] + ' '
      + TwoDigitString(LOCAL_DAY(date)) + ', '
      + LOCAL_YEAR(date);
}


function TimeString(date) {
  CHECK_DATE(date);
  return TwoDigitString(LOCAL_HOUR(date)) + ':'
      + TwoDigitString(LOCAL_MIN(date)) + ':'
      + TwoDigitString(LOCAL_SEC(date));
}


function TimeStringUTC(date) {
  CHECK_DATE(date);
  return TwoDigitString(UTC_HOUR(date)) + ':'
      + TwoDigitString(UTC_MIN(date)) + ':'
      + TwoDigitString(UTC_SEC(date));
}


function LocalTimezoneString(date) {
  CHECK_DATE(date);
  var timezone = LocalTimezone(UTC_DATE_VALUE(date));

  var timezoneOffset = -TIMEZONE_OFFSET(date);
  var sign = (timezoneOffset >= 0) ? 1 : -1;
  var hours = MathFloor((sign * timezoneOffset)/60);
  var min   = MathFloor((sign * timezoneOffset)%60);
  var gmt = ' GMT' + ((sign == 1) ? '+' : '-') +
      TwoDigitString(hours) + TwoDigitString(min);
  return gmt + ' (' +  timezone + ')';
}


function DatePrintString(date) {
  CHECK_DATE(date);
  return DateString(date) + ' ' + TimeString(date);
}

// -------------------------------------------------------------------

// Reused output buffer. Used when parsing date strings.
var parse_buffer = new InternalArray(8);

// ECMA 262 - 15.9.4.2
function DateParse(string) {
  var arr = %DateParseString($toString(string), parse_buffer);
  if (IS_NULL(arr)) return NAN;

  var day = MakeDay(arr[0], arr[1], arr[2]);
  var time = MakeTime(arr[3], arr[4], arr[5], arr[6]);
  var date = MakeDate(day, time);

  if (IS_NULL(arr[7])) {
    return TimeClip(UTC(date));
  } else {
    return TimeClip(date - arr[7] * 1000);
  }
}


// ECMA 262 - 15.9.4.3
function DateUTC(year, month, date, hours, minutes, seconds, ms) {
  year = $toNumber(year);
  month = $toNumber(month);
  var argc = %_ArgumentsLength();
  date = argc > 2 ? $toNumber(date) : 1;
  hours = argc > 3 ? $toNumber(hours) : 0;
  minutes = argc > 4 ? $toNumber(minutes) : 0;
  seconds = argc > 5 ? $toNumber(seconds) : 0;
  ms = argc > 6 ? $toNumber(ms) : 0;
  year = (!NUMBER_IS_NAN(year) &&
          0 <= TO_INTEGER(year) &&
          TO_INTEGER(year) <= 99) ? 1900 + TO_INTEGER(year) : year;
  var day = MakeDay(year, month, date);
  var time = MakeTime(hours, minutes, seconds, ms);
  return TimeClip(MakeDate(day, time));
}


// ECMA 262 - 15.9.4.4
function DateNow() {
  return %DateCurrentTime();
}


// ECMA 262 - 15.9.5.2
function DateToString() {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this)
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  var time_zone_string = LocalTimezoneString(this)
  return DatePrintString(this) + time_zone_string;
}


// ECMA 262 - 15.9.5.3
function DateToDateString() {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  return DateString(this);
}


// ECMA 262 - 15.9.5.4
function DateToTimeString() {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  var time_zone_string = LocalTimezoneString(this);
  return TimeString(this) + time_zone_string;
}


// ECMA 262 - 15.9.5.5
function DateToLocaleString() {
  CHECK_DATE(this);
  return %_CallFunction(this, DateToString);
}


// ECMA 262 - 15.9.5.6
function DateToLocaleDateString() {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  return LongDateString(this);
}


// ECMA 262 - 15.9.5.7
function DateToLocaleTimeString() {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  return TimeString(this);
}


// ECMA 262 - 15.9.5.8
function DateValueOf() {
  CHECK_DATE(this);
  return UTC_DATE_VALUE(this);
}


// ECMA 262 - 15.9.5.9
function DateGetTime() {
  CHECK_DATE(this);
  return UTC_DATE_VALUE(this);
}


// ECMA 262 - 15.9.5.10
function DateGetFullYear() {
  CHECK_DATE(this);
  return LOCAL_YEAR(this);
}


// ECMA 262 - 15.9.5.11
function DateGetUTCFullYear() {
  CHECK_DATE(this);
  return UTC_YEAR(this);
}


// ECMA 262 - 15.9.5.12
function DateGetMonth() {
  CHECK_DATE(this);
  return LOCAL_MONTH(this);
}


// ECMA 262 - 15.9.5.13
function DateGetUTCMonth() {
  CHECK_DATE(this);
  return UTC_MONTH(this);
}


// ECMA 262 - 15.9.5.14
function DateGetDate() {
  CHECK_DATE(this);
  return LOCAL_DAY(this);
}


// ECMA 262 - 15.9.5.15
function DateGetUTCDate() {
  CHECK_DATE(this);
  return UTC_DAY(this);
}


// ECMA 262 - 15.9.5.16
function DateGetDay() {
  CHECK_DATE(this);
  return LOCAL_WEEKDAY(this);
}


// ECMA 262 - 15.9.5.17
function DateGetUTCDay() {
  CHECK_DATE(this);
  return UTC_WEEKDAY(this);
}


// ECMA 262 - 15.9.5.18
function DateGetHours() {
  CHECK_DATE(this);
  return LOCAL_HOUR(this);
}


// ECMA 262 - 15.9.5.19
function DateGetUTCHours() {
  CHECK_DATE(this);
  return UTC_HOUR(this);
}


// ECMA 262 - 15.9.5.20
function DateGetMinutes() {
  CHECK_DATE(this);
  return LOCAL_MIN(this);
}


// ECMA 262 - 15.9.5.21
function DateGetUTCMinutes() {
  CHECK_DATE(this);
  return UTC_MIN(this);
}


// ECMA 262 - 15.9.5.22
function DateGetSeconds() {
  CHECK_DATE(this);
  return LOCAL_SEC(this);
}


// ECMA 262 - 15.9.5.23
function DateGetUTCSeconds() {
  CHECK_DATE(this);
  return UTC_SEC(this)
}


// ECMA 262 - 15.9.5.24
function DateGetMilliseconds() {
  CHECK_DATE(this);
  return LOCAL_MS(this);
}


// ECMA 262 - 15.9.5.25
function DateGetUTCMilliseconds() {
  CHECK_DATE(this);
  return UTC_MS(this);
}


// ECMA 262 - 15.9.5.26
function DateGetTimezoneOffset() {
  CHECK_DATE(this);
  return TIMEZONE_OFFSET(this);
}


// ECMA 262 - 15.9.5.27
function DateSetTime(ms) {
  CHECK_DATE(this);
  SET_UTC_DATE_VALUE(this, $toNumber(ms));
  return UTC_DATE_VALUE(this);
}


// ECMA 262 - 15.9.5.28
function DateSetMilliseconds(ms) {
  CHECK_DATE(this);
  var t = LOCAL_DATE_VALUE(this);
  ms = $toNumber(ms);
  var time = MakeTime(LOCAL_HOUR(this), LOCAL_MIN(this), LOCAL_SEC(this), ms);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(LOCAL_DAYS(this), time));
}


// ECMA 262 - 15.9.5.29
function DateSetUTCMilliseconds(ms) {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  ms = $toNumber(ms);
  var time = MakeTime(UTC_HOUR(this),
                      UTC_MIN(this),
                      UTC_SEC(this),
                      ms);
  return SET_UTC_DATE_VALUE(this, MakeDate(UTC_DAYS(this), time));
}


// ECMA 262 - 15.9.5.30
function DateSetSeconds(sec, ms) {
  CHECK_DATE(this);
  var t = LOCAL_DATE_VALUE(this);
  sec = $toNumber(sec);
  ms = %_ArgumentsLength() < 2 ? LOCAL_MS(this) : $toNumber(ms);
  var time = MakeTime(LOCAL_HOUR(this), LOCAL_MIN(this), sec, ms);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(LOCAL_DAYS(this), time));
}


// ECMA 262 - 15.9.5.31
function DateSetUTCSeconds(sec, ms) {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  sec = $toNumber(sec);
  ms = %_ArgumentsLength() < 2 ? UTC_MS(this) : $toNumber(ms);
  var time = MakeTime(UTC_HOUR(this), UTC_MIN(this), sec, ms);
  return SET_UTC_DATE_VALUE(this, MakeDate(UTC_DAYS(this), time));
}


// ECMA 262 - 15.9.5.33
function DateSetMinutes(min, sec, ms) {
  CHECK_DATE(this);
  var t = LOCAL_DATE_VALUE(this);
  min = $toNumber(min);
  var argc = %_ArgumentsLength();
  sec = argc < 2 ? LOCAL_SEC(this) : $toNumber(sec);
  ms = argc < 3 ? LOCAL_MS(this) : $toNumber(ms);
  var time = MakeTime(LOCAL_HOUR(this), min, sec, ms);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(LOCAL_DAYS(this), time));
}


// ECMA 262 - 15.9.5.34
function DateSetUTCMinutes(min, sec, ms) {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  min = $toNumber(min);
  var argc = %_ArgumentsLength();
  sec = argc < 2 ? UTC_SEC(this) : $toNumber(sec);
  ms = argc < 3 ? UTC_MS(this) : $toNumber(ms);
  var time = MakeTime(UTC_HOUR(this), min, sec, ms);
  return SET_UTC_DATE_VALUE(this, MakeDate(UTC_DAYS(this), time));
}


// ECMA 262 - 15.9.5.35
function DateSetHours(hour, min, sec, ms) {
  CHECK_DATE(this);
  var t = LOCAL_DATE_VALUE(this);
  hour = $toNumber(hour);
  var argc = %_ArgumentsLength();
  min = argc < 2 ? LOCAL_MIN(this) : $toNumber(min);
  sec = argc < 3 ? LOCAL_SEC(this) : $toNumber(sec);
  ms = argc < 4 ? LOCAL_MS(this) : $toNumber(ms);
  var time = MakeTime(hour, min, sec, ms);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(LOCAL_DAYS(this), time));
}


// ECMA 262 - 15.9.5.34
function DateSetUTCHours(hour, min, sec, ms) {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  hour = $toNumber(hour);
  var argc = %_ArgumentsLength();
  min = argc < 2 ? UTC_MIN(this) : $toNumber(min);
  sec = argc < 3 ? UTC_SEC(this) : $toNumber(sec);
  ms = argc < 4 ? UTC_MS(this) : $toNumber(ms);
  var time = MakeTime(hour, min, sec, ms);
  return SET_UTC_DATE_VALUE(this, MakeDate(UTC_DAYS(this), time));
}


// ECMA 262 - 15.9.5.36
function DateSetDate(date) {
  CHECK_DATE(this);
  var t = LOCAL_DATE_VALUE(this);
  date = $toNumber(date);
  var day = MakeDay(LOCAL_YEAR(this), LOCAL_MONTH(this), date);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(day, LOCAL_TIME_IN_DAY(this)));
}


// ECMA 262 - 15.9.5.37
function DateSetUTCDate(date) {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  date = $toNumber(date);
  var day = MakeDay(UTC_YEAR(this), UTC_MONTH(this), date);
  return SET_UTC_DATE_VALUE(this, MakeDate(day, UTC_TIME_IN_DAY(this)));
}


// ECMA 262 - 15.9.5.38
function DateSetMonth(month, date) {
  CHECK_DATE(this);
  var t = LOCAL_DATE_VALUE(this);
  month = $toNumber(month);
  date = %_ArgumentsLength() < 2 ? LOCAL_DAY(this) : $toNumber(date);
  var day = MakeDay(LOCAL_YEAR(this), month, date);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(day, LOCAL_TIME_IN_DAY(this)));
}


// ECMA 262 - 15.9.5.39
function DateSetUTCMonth(month, date) {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  month = $toNumber(month);
  date = %_ArgumentsLength() < 2 ? UTC_DAY(this) : $toNumber(date);
  var day = MakeDay(UTC_YEAR(this), month, date);
  return SET_UTC_DATE_VALUE(this, MakeDate(day, UTC_TIME_IN_DAY(this)));
}


// ECMA 262 - 15.9.5.40
function DateSetFullYear(year, month, date) {
  CHECK_DATE(this);
  var t = LOCAL_DATE_VALUE(this);
  year = $toNumber(year);
  var argc = %_ArgumentsLength();
  var time ;
  if (NUMBER_IS_NAN(t)) {
    month = argc < 2 ? 0 : $toNumber(month);
    date = argc < 3 ? 1 : $toNumber(date);
    time = 0;
  } else {
    month = argc < 2 ? LOCAL_MONTH(this) : $toNumber(month);
    date = argc < 3 ? LOCAL_DAY(this) : $toNumber(date);
    time = LOCAL_TIME_IN_DAY(this);
  }
  var day = MakeDay(year, month, date);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(day, time));
}


// ECMA 262 - 15.9.5.41
function DateSetUTCFullYear(year, month, date) {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  year = $toNumber(year);
  var argc = %_ArgumentsLength();
  var time ;
  if (NUMBER_IS_NAN(t)) {
    month = argc < 2 ? 0 : $toNumber(month);
    date = argc < 3 ? 1 : $toNumber(date);
    time = 0;
  } else {
    month = argc < 2 ? UTC_MONTH(this) : $toNumber(month);
    date = argc < 3 ? UTC_DAY(this) : $toNumber(date);
    time = UTC_TIME_IN_DAY(this);
  }
  var day = MakeDay(year, month, date);
  return SET_UTC_DATE_VALUE(this, MakeDate(day, time));
}


// ECMA 262 - 15.9.5.42
function DateToUTCString() {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  // Return UTC string of the form: Sat, 31 Jan 1970 23:00:00 GMT
  return WeekDays[UTC_WEEKDAY(this)] + ', '
      + TwoDigitString(UTC_DAY(this)) + ' '
      + Months[UTC_MONTH(this)] + ' '
      + UTC_YEAR(this) + ' '
      + TimeStringUTC(this) + ' GMT';
}


// ECMA 262 - B.2.4
function DateGetYear() {
  CHECK_DATE(this);
  return LOCAL_YEAR(this) - 1900;
}


// ECMA 262 - B.2.5
function DateSetYear(year) {
  CHECK_DATE(this);
  year = $toNumber(year);
  if (NUMBER_IS_NAN(year)) return SET_UTC_DATE_VALUE(this, NAN);
  year = (0 <= TO_INTEGER(year) && TO_INTEGER(year) <= 99)
      ? 1900 + TO_INTEGER(year) : year;
  var t = LOCAL_DATE_VALUE(this);
  var month, date, time;
  if (NUMBER_IS_NAN(t))  {
    month = 0;
    date = 1;
    time = 0;
  } else {
    month = LOCAL_MONTH(this);
    date = LOCAL_DAY(this);
    time = LOCAL_TIME_IN_DAY(this);
  }
  var day = MakeDay(year, month, date);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(day, time));
}


// ECMA 262 - B.2.6
//
// Notice that this does not follow ECMA 262 completely.  ECMA 262
// says that toGMTString should be the same Function object as
// toUTCString.  JSC does not do this, so for compatibility we do not
// do that either.  Instead, we create a new function whose name
// property will return toGMTString.
function DateToGMTString() {
  return %_CallFunction(this, DateToUTCString);
}


function PadInt(n, digits) {
  if (digits == 1) return n;
  return n < %_MathPow(10, digits - 1) ? '0' + PadInt(n, digits - 1) : n;
}


// ECMA 262 - 20.3.4.36
function DateToISOString() {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) throw MakeRangeError(kInvalidTimeValue);
  var year = UTC_YEAR(this);
  var year_string;
  if (year >= 0 && year <= 9999) {
    year_string = PadInt(year, 4);
  } else {
    if (year < 0) {
      year_string = "-" + PadInt(-year, 6);
    } else {
      year_string = "+" + PadInt(year, 6);
    }
  }
  return year_string +
      '-' + PadInt(UTC_MONTH(this) + 1, 2) +
      '-' + PadInt(UTC_DAY(this), 2) +
      'T' + PadInt(UTC_HOUR(this), 2) +
      ':' + PadInt(UTC_MIN(this), 2) +
      ':' + PadInt(UTC_SEC(this), 2) +
      '.' + PadInt(UTC_MS(this), 3) +
      'Z';
}


function DateToJSON(key) {
  var o = $toObject(this);
  var tv = $defaultNumber(o);
  if (IS_NUMBER(tv) && !NUMBER_IS_FINITE(tv)) {
    return null;
  }
  return o.toISOString();
}


var date_cache_version_holder;
var date_cache_version = NAN;


function CheckDateCacheCurrent() {
  if (!date_cache_version_holder) {
    date_cache_version_holder = %DateCacheVersion();
    if (!date_cache_version_holder) return;
  }
  if (date_cache_version_holder[0] == date_cache_version) {
    return;
  }
  date_cache_version = date_cache_version_holder[0];

  // Reset the timezone cache:
  timezone_cache_time = NAN;
  timezone_cache_timezone = UNDEFINED;

  // Reset the date cache:
  Date_cache.time = NAN;
  Date_cache.string = null;
}


function CreateDate(time) {
  var date = new GlobalDate();
  date.setTime(time);
  return date;
}

// -------------------------------------------------------------------

%SetCode(GlobalDate, DateConstructor);
%FunctionSetPrototype(GlobalDate, new GlobalDate(NAN));

// Set up non-enumerable properties of the Date object itself.
utils.InstallFunctions(GlobalDate, DONT_ENUM, [
  "UTC", DateUTC,
  "parse", DateParse,
  "now", DateNow
]);

// Set up non-enumerable constructor property of the Date prototype object.
%AddNamedProperty(GlobalDate.prototype, "constructor", GlobalDate, DONT_ENUM);

// Set up non-enumerable functions of the Date prototype object and
// set their names.
utils.InstallFunctions(GlobalDate.prototype, DONT_ENUM, [
  "toString", DateToString,
  "toDateString", DateToDateString,
  "toTimeString", DateToTimeString,
  "toLocaleString", DateToLocaleString,
  "toLocaleDateString", DateToLocaleDateString,
  "toLocaleTimeString", DateToLocaleTimeString,
  "valueOf", DateValueOf,
  "getTime", DateGetTime,
  "getFullYear", DateGetFullYear,
  "getUTCFullYear", DateGetUTCFullYear,
  "getMonth", DateGetMonth,
  "getUTCMonth", DateGetUTCMonth,
  "getDate", DateGetDate,
  "getUTCDate", DateGetUTCDate,
  "getDay", DateGetDay,
  "getUTCDay", DateGetUTCDay,
  "getHours", DateGetHours,
  "getUTCHours", DateGetUTCHours,
  "getMinutes", DateGetMinutes,
  "getUTCMinutes", DateGetUTCMinutes,
  "getSeconds", DateGetSeconds,
  "getUTCSeconds", DateGetUTCSeconds,
  "getMilliseconds", DateGetMilliseconds,
  "getUTCMilliseconds", DateGetUTCMilliseconds,
  "getTimezoneOffset", DateGetTimezoneOffset,
  "setTime", DateSetTime,
  "setMilliseconds", DateSetMilliseconds,
  "setUTCMilliseconds", DateSetUTCMilliseconds,
  "setSeconds", DateSetSeconds,
  "setUTCSeconds", DateSetUTCSeconds,
  "setMinutes", DateSetMinutes,
  "setUTCMinutes", DateSetUTCMinutes,
  "setHours", DateSetHours,
  "setUTCHours", DateSetUTCHours,
  "setDate", DateSetDate,
  "setUTCDate", DateSetUTCDate,
  "setMonth", DateSetMonth,
  "setUTCMonth", DateSetUTCMonth,
  "setFullYear", DateSetFullYear,
  "setUTCFullYear", DateSetUTCFullYear,
  "toGMTString", DateToGMTString,
  "toUTCString", DateToUTCString,
  "getYear", DateGetYear,
  "setYear", DateSetYear,
  "toISOString", DateToISOString,
  "toJSON", DateToJSON
]);

// Expose to the global scope.
$createDate = CreateDate;

})
