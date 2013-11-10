// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This file relies on the fact that the following declarations have been made
// in v8natives.js:
// var $isFinite = GlobalIsFinite;

var $Date = global.Date;

// -------------------------------------------------------------------

// This file contains date support implemented in JavaScript.

// Helper function to throw error.
function ThrowDateTypeError() {
  throw new $TypeError('this is not a Date object.');
}


var timezone_cache_time = NAN;
var timezone_cache_timezone;

function LocalTimezone(t) {
  if (NUMBER_IS_NAN(t)) return "";
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
  if (!$isFinite(hour)) return NAN;
  if (!$isFinite(min)) return NAN;
  if (!$isFinite(sec)) return NAN;
  if (!$isFinite(ms)) return NAN;
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
  if (!$isFinite(year) || !$isFinite(month) || !$isFinite(date)) return NAN;

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
  if ($abs(time) > MAX_TIME_BEFORE_UTC) return NAN;
  return time;
}


// ECMA 262 - 15.9.1.14
function TimeClip(time) {
  if (!$isFinite(time)) return NAN;
  if ($abs(time) > MAX_TIME_MS) return NAN;
  return TO_INTEGER(time);
}


// The Date cache is used to limit the cost of parsing the same Date
// strings over and over again.
var Date_cache = {
  // Cached time value.
  time: NAN,
  // String input for which the cached time is valid.
  string: null
};


function DateConstructor(year, month, date, hours, minutes, seconds, ms) {
  if (!%_IsConstructCall()) {
    // ECMA 262 - 15.9.2
    return (new $Date()).toString();
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
      var time = ToPrimitive(year, NUMBER_HINT);
      value = IS_STRING(time) ? DateParse(time) : ToNumber(time);
    }
    SET_UTC_DATE_VALUE(this, value);
  } else {
    year = ToNumber(year);
    month = ToNumber(month);
    date = argc > 2 ? ToNumber(date) : 1;
    hours = argc > 3 ? ToNumber(hours) : 0;
    minutes = argc > 4 ? ToNumber(minutes) : 0;
    seconds = argc > 5 ? ToNumber(seconds) : 0;
    ms = argc > 6 ? ToNumber(ms) : 0;
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
  return LongWeekDays[LOCAL_WEEKDAY(date)] + ', '
      + LongMonths[LOCAL_MONTH(date)] + ' '
      + TwoDigitString(LOCAL_DAY(date)) + ', '
      + LOCAL_YEAR(date);
}


function TimeString(date) {
  return TwoDigitString(LOCAL_HOUR(date)) + ':'
      + TwoDigitString(LOCAL_MIN(date)) + ':'
      + TwoDigitString(LOCAL_SEC(date));
}


function TimeStringUTC(date) {
  return TwoDigitString(UTC_HOUR(date)) + ':'
      + TwoDigitString(UTC_MIN(date)) + ':'
      + TwoDigitString(UTC_SEC(date));
}


function LocalTimezoneString(date) {
  var timezone = LocalTimezone(UTC_DATE_VALUE(date));

  var timezoneOffset = -TIMEZONE_OFFSET(date);
  var sign = (timezoneOffset >= 0) ? 1 : -1;
  var hours = FLOOR((sign * timezoneOffset)/60);
  var min   = FLOOR((sign * timezoneOffset)%60);
  var gmt = ' GMT' + ((sign == 1) ? '+' : '-') +
      TwoDigitString(hours) + TwoDigitString(min);
  return gmt + ' (' +  timezone + ')';
}


function DatePrintString(date) {
  return DateString(date) + ' ' + TimeString(date);
}

// -------------------------------------------------------------------

// Reused output buffer. Used when parsing date strings.
var parse_buffer = $Array(8);

// ECMA 262 - 15.9.4.2
function DateParse(string) {
  var arr = %DateParseString(ToString(string), parse_buffer);
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
  year = ToNumber(year);
  month = ToNumber(month);
  var argc = %_ArgumentsLength();
  date = argc > 2 ? ToNumber(date) : 1;
  hours = argc > 3 ? ToNumber(hours) : 0;
  minutes = argc > 4 ? ToNumber(minutes) : 0;
  seconds = argc > 5 ? ToNumber(seconds) : 0;
  ms = argc > 6 ? ToNumber(ms) : 0;
  year = (!NUMBER_IS_NAN(year) &&
          0 <= TO_INTEGER(year) &&
          TO_INTEGER(year) <= 99) ? 1900 + TO_INTEGER(year) : year;
  var day = MakeDay(year, month, date);
  var time = MakeTime(hours, minutes, seconds, ms);
  return TimeClip(MakeDate(day, time));
}


// Mozilla-specific extension. Returns the number of milliseconds
// elapsed since 1 January 1970 00:00:00 UTC.
function DateNow() {
  return %DateCurrentTime();
}


// ECMA 262 - 15.9.5.2
function DateToString() {
  var t = UTC_DATE_VALUE(this)
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  var time_zone_string = LocalTimezoneString(this)
  return DatePrintString(this) + time_zone_string;
}


// ECMA 262 - 15.9.5.3
function DateToDateString() {
  var t = UTC_DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  return DateString(this);
}


// ECMA 262 - 15.9.5.4
function DateToTimeString() {
  var t = UTC_DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  var time_zone_string = LocalTimezoneString(this);
  return TimeString(this) + time_zone_string;
}


// ECMA 262 - 15.9.5.5
function DateToLocaleString() {
  return %_CallFunction(this, DateToString);
}


// ECMA 262 - 15.9.5.6
function DateToLocaleDateString() {
  var t = UTC_DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  return LongDateString(this);
}


// ECMA 262 - 15.9.5.7
function DateToLocaleTimeString() {
  var t = UTC_DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  return TimeString(this);
}


// ECMA 262 - 15.9.5.8
function DateValueOf() {
  return UTC_DATE_VALUE(this);
}


// ECMA 262 - 15.9.5.9
function DateGetTime() {
  return UTC_DATE_VALUE(this);
}


// ECMA 262 - 15.9.5.10
function DateGetFullYear() {
  return LOCAL_YEAR(this);
}


// ECMA 262 - 15.9.5.11
function DateGetUTCFullYear() {
  return UTC_YEAR(this);
}


// ECMA 262 - 15.9.5.12
function DateGetMonth() {
  return LOCAL_MONTH(this);
}


// ECMA 262 - 15.9.5.13
function DateGetUTCMonth() {
  return UTC_MONTH(this);
}


// ECMA 262 - 15.9.5.14
function DateGetDate() {
  return LOCAL_DAY(this);
}


// ECMA 262 - 15.9.5.15
function DateGetUTCDate() {
  return UTC_DAY(this);
}


// ECMA 262 - 15.9.5.16
function DateGetDay() {
  return LOCAL_WEEKDAY(this);
}


// ECMA 262 - 15.9.5.17
function DateGetUTCDay() {
  return UTC_WEEKDAY(this);
}


// ECMA 262 - 15.9.5.18
function DateGetHours() {
  return LOCAL_HOUR(this);
}


// ECMA 262 - 15.9.5.19
function DateGetUTCHours() {
  return UTC_HOUR(this);
}


// ECMA 262 - 15.9.5.20
function DateGetMinutes() {
  return LOCAL_MIN(this);
}


// ECMA 262 - 15.9.5.21
function DateGetUTCMinutes() {
  return UTC_MIN(this);
}


// ECMA 262 - 15.9.5.22
function DateGetSeconds() {
  return LOCAL_SEC(this);
}


// ECMA 262 - 15.9.5.23
function DateGetUTCSeconds() {
  return UTC_SEC(this)
}


// ECMA 262 - 15.9.5.24
function DateGetMilliseconds() {
  return LOCAL_MS(this);
}


// ECMA 262 - 15.9.5.25
function DateGetUTCMilliseconds() {
  return UTC_MS(this);
}


// ECMA 262 - 15.9.5.26
function DateGetTimezoneOffset() {
  return TIMEZONE_OFFSET(this);
}


// ECMA 262 - 15.9.5.27
function DateSetTime(ms) {
  CHECK_DATE(this);
  SET_UTC_DATE_VALUE(this, ToNumber(ms));
  return UTC_DATE_VALUE(this);
}


// ECMA 262 - 15.9.5.28
function DateSetMilliseconds(ms) {
  var t = LOCAL_DATE_VALUE(this);
  ms = ToNumber(ms);
  var time = MakeTime(LOCAL_HOUR(this), LOCAL_MIN(this), LOCAL_SEC(this), ms);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(LOCAL_DAYS(this), time));
}


// ECMA 262 - 15.9.5.29
function DateSetUTCMilliseconds(ms) {
  var t = UTC_DATE_VALUE(this);
  ms = ToNumber(ms);
  var time = MakeTime(UTC_HOUR(this),
                      UTC_MIN(this),
                      UTC_SEC(this),
                      ms);
  return SET_UTC_DATE_VALUE(this, MakeDate(UTC_DAYS(this), time));
}


// ECMA 262 - 15.9.5.30
function DateSetSeconds(sec, ms) {
  var t = LOCAL_DATE_VALUE(this);
  sec = ToNumber(sec);
  ms = %_ArgumentsLength() < 2 ? LOCAL_MS(this) : ToNumber(ms);
  var time = MakeTime(LOCAL_HOUR(this), LOCAL_MIN(this), sec, ms);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(LOCAL_DAYS(this), time));
}


// ECMA 262 - 15.9.5.31
function DateSetUTCSeconds(sec, ms) {
  var t = UTC_DATE_VALUE(this);
  sec = ToNumber(sec);
  ms = %_ArgumentsLength() < 2 ? UTC_MS(this) : ToNumber(ms);
  var time = MakeTime(UTC_HOUR(this), UTC_MIN(this), sec, ms);
  return SET_UTC_DATE_VALUE(this, MakeDate(UTC_DAYS(this), time));
}


// ECMA 262 - 15.9.5.33
function DateSetMinutes(min, sec, ms) {
  var t = LOCAL_DATE_VALUE(this);
  min = ToNumber(min);
  var argc = %_ArgumentsLength();
  sec = argc < 2 ? LOCAL_SEC(this) : ToNumber(sec);
  ms = argc < 3 ? LOCAL_MS(this) : ToNumber(ms);
  var time = MakeTime(LOCAL_HOUR(this), min, sec, ms);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(LOCAL_DAYS(this), time));
}


// ECMA 262 - 15.9.5.34
function DateSetUTCMinutes(min, sec, ms) {
  var t = UTC_DATE_VALUE(this);
  min = ToNumber(min);
  var argc = %_ArgumentsLength();
  sec = argc < 2 ? UTC_SEC(this) : ToNumber(sec);
  ms = argc < 3 ? UTC_MS(this) : ToNumber(ms);
  var time = MakeTime(UTC_HOUR(this), min, sec, ms);
  return SET_UTC_DATE_VALUE(this, MakeDate(UTC_DAYS(this), time));
}


// ECMA 262 - 15.9.5.35
function DateSetHours(hour, min, sec, ms) {
  var t = LOCAL_DATE_VALUE(this);
  hour = ToNumber(hour);
  var argc = %_ArgumentsLength();
  min = argc < 2 ? LOCAL_MIN(this) : ToNumber(min);
  sec = argc < 3 ? LOCAL_SEC(this) : ToNumber(sec);
  ms = argc < 4 ? LOCAL_MS(this) : ToNumber(ms);
  var time = MakeTime(hour, min, sec, ms);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(LOCAL_DAYS(this), time));
}


// ECMA 262 - 15.9.5.34
function DateSetUTCHours(hour, min, sec, ms) {
  var t = UTC_DATE_VALUE(this);
  hour = ToNumber(hour);
  var argc = %_ArgumentsLength();
  min = argc < 2 ? UTC_MIN(this) : ToNumber(min);
  sec = argc < 3 ? UTC_SEC(this) : ToNumber(sec);
  ms = argc < 4 ? UTC_MS(this) : ToNumber(ms);
  var time = MakeTime(hour, min, sec, ms);
  return SET_UTC_DATE_VALUE(this, MakeDate(UTC_DAYS(this), time));
}


// ECMA 262 - 15.9.5.36
function DateSetDate(date) {
  var t = LOCAL_DATE_VALUE(this);
  date = ToNumber(date);
  var day = MakeDay(LOCAL_YEAR(this), LOCAL_MONTH(this), date);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(day, LOCAL_TIME_IN_DAY(this)));
}


// ECMA 262 - 15.9.5.37
function DateSetUTCDate(date) {
  var t = UTC_DATE_VALUE(this);
  date = ToNumber(date);
  var day = MakeDay(UTC_YEAR(this), UTC_MONTH(this), date);
  return SET_UTC_DATE_VALUE(this, MakeDate(day, UTC_TIME_IN_DAY(this)));
}


// ECMA 262 - 15.9.5.38
function DateSetMonth(month, date) {
  var t = LOCAL_DATE_VALUE(this);
  month = ToNumber(month);
  date = %_ArgumentsLength() < 2 ? LOCAL_DAY(this) : ToNumber(date);
  var day = MakeDay(LOCAL_YEAR(this), month, date);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(day, LOCAL_TIME_IN_DAY(this)));
}


// ECMA 262 - 15.9.5.39
function DateSetUTCMonth(month, date) {
  var t = UTC_DATE_VALUE(this);
  month = ToNumber(month);
  date = %_ArgumentsLength() < 2 ? UTC_DAY(this) : ToNumber(date);
  var day = MakeDay(UTC_YEAR(this), month, date);
  return SET_UTC_DATE_VALUE(this, MakeDate(day, UTC_TIME_IN_DAY(this)));
}


// ECMA 262 - 15.9.5.40
function DateSetFullYear(year, month, date) {
  var t = LOCAL_DATE_VALUE(this);
  year = ToNumber(year);
  var argc = %_ArgumentsLength();
  var time ;
  if (NUMBER_IS_NAN(t)) {
    month = argc < 2 ? 0 : ToNumber(month);
    date = argc < 3 ? 1 : ToNumber(date);
    time = 0;
  } else {
    month = argc < 2 ? LOCAL_MONTH(this) : ToNumber(month);
    date = argc < 3 ? LOCAL_DAY(this) : ToNumber(date);
    time = LOCAL_TIME_IN_DAY(this);
  }
  var day = MakeDay(year, month, date);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(day, time));
}


// ECMA 262 - 15.9.5.41
function DateSetUTCFullYear(year, month, date) {
  var t = UTC_DATE_VALUE(this);
  year = ToNumber(year);
  var argc = %_ArgumentsLength();
  var time ;
  if (NUMBER_IS_NAN(t)) {
    month = argc < 2 ? 0 : ToNumber(month);
    date = argc < 3 ? 1 : ToNumber(date);
    time = 0;
  } else {
    month = argc < 2 ? UTC_MONTH(this) : ToNumber(month);
    date = argc < 3 ? UTC_DAY(this) : ToNumber(date);
    time = UTC_TIME_IN_DAY(this);
  }
  var day = MakeDay(year, month, date);
  return SET_UTC_DATE_VALUE(this, MakeDate(day, time));
}


// ECMA 262 - 15.9.5.42
function DateToUTCString() {
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
  return LOCAL_YEAR(this) - 1900;
}


// ECMA 262 - B.2.5
function DateSetYear(year) {
  CHECK_DATE(this);
  year = ToNumber(year);
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
  return n < MathPow(10, digits - 1) ? '0' + PadInt(n, digits - 1) : n;
}


// ECMA 262 - 15.9.5.43
function DateToISOString() {
  var t = UTC_DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) throw MakeRangeError("invalid_time_value", []);
  var year = this.getUTCFullYear();
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
      '-' + PadInt(this.getUTCMonth() + 1, 2) +
      '-' + PadInt(this.getUTCDate(), 2) +
      'T' + PadInt(this.getUTCHours(), 2) +
      ':' + PadInt(this.getUTCMinutes(), 2) +
      ':' + PadInt(this.getUTCSeconds(), 2) +
      '.' + PadInt(this.getUTCMilliseconds(), 3) +
      'Z';
}


function DateToJSON(key) {
  var o = ToObject(this);
  var tv = DefaultNumber(o);
  if (IS_NUMBER(tv) && !NUMBER_IS_FINITE(tv)) {
    return null;
  }
  return o.toISOString();
}


function ResetDateCache() {
  // Reset the timezone cache:
  timezone_cache_time = NAN;
  timezone_cache_timezone = undefined;

  // Reset the date cache:
  cache = Date_cache;
  cache.time = NAN;
  cache.string = null;
}


// -------------------------------------------------------------------

function SetUpDate() {
  %CheckIsBootstrapping();

  %SetCode($Date, DateConstructor);
  %FunctionSetPrototype($Date, new $Date(NAN));

  // Set up non-enumerable properties of the Date object itself.
  InstallFunctions($Date, DONT_ENUM, $Array(
    "UTC", DateUTC,
    "parse", DateParse,
    "now", DateNow
  ));

  // Set up non-enumerable constructor property of the Date prototype object.
  %SetProperty($Date.prototype, "constructor", $Date, DONT_ENUM);

  // Set up non-enumerable functions of the Date prototype object and
  // set their names.
  InstallFunctions($Date.prototype, DONT_ENUM, $Array(
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
  ));
}

SetUpDate();
