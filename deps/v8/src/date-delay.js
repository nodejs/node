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
// const $isFinite = GlobalIsFinite;

// -------------------------------------------------------------------

// This file contains date support implemented in JavaScript.


// Keep reference to original values of some global properties.  This
// has the added benefit that the code in this file is isolated from
// changes to these properties.
const $Date = global.Date;

// Helper function to throw error.
function ThrowDateTypeError() {
  throw new $TypeError('this is not a Date object.');
}

// ECMA 262 - 15.9.1.2
function Day(time) {
  return FLOOR(time / msPerDay);
}


// ECMA 262 - 5.2
function Modulo(value, remainder) {
  var mod = value % remainder;
  // Guard against returning -0.
  if (mod == 0) return 0;
  return mod >= 0 ? mod : mod + remainder;
}


function TimeWithinDay(time) {
  return Modulo(time, msPerDay);
}


// ECMA 262 - 15.9.1.3
function DaysInYear(year) {
  if (year % 4 != 0) return 365;
  if ((year % 100 == 0) && (year % 400 != 0)) return 365;
  return 366;
}


function DayFromYear(year) {
  return 365 * (year-1970)
      + FLOOR((year-1969)/4)
      - FLOOR((year-1901)/100)
      + FLOOR((year-1601)/400);
}


function TimeFromYear(year) {
  return msPerDay * DayFromYear(year);
}


function YearFromTime(time) {
  return FromJulianDay(Day(time) + kDayZeroInJulianDay).year;
}


function InLeapYear(time) {
  return DaysInYear(YearFromTime(time)) == 366 ? 1 : 0;
}


// ECMA 262 - 15.9.1.4
function MonthFromTime(time) {
  return FromJulianDay(Day(time) + kDayZeroInJulianDay).month;
}


function DayWithinYear(time) {
  return Day(time) - DayFromYear(YearFromTime(time));
}


// ECMA 262 - 15.9.1.5
function DateFromTime(time) {
  return FromJulianDay(Day(time) + kDayZeroInJulianDay).date;
}


// ECMA 262 - 15.9.1.9
function EquivalentYear(year) {
  // Returns an equivalent year in the range [2008-2035] matching
  // - leap year.
  // - week day of first day.
  var time = TimeFromYear(year);
  var recent_year = (InLeapYear(time) == 0 ? 1967 : 1956) +
      (WeekDay(time) * 12) % 28;
  // Find the year in the range 2008..2037 that is equivalent mod 28.
  // Add 3*28 to give a positive argument to the modulus operator.
  return 2008 + (recent_year + 3*28 - 2008) % 28;
}


function EquivalentTime(t) {
  // The issue here is that some library calls don't work right for dates
  // that cannot be represented using a non-negative signed 32 bit integer
  // (measured in whole seconds based on the 1970 epoch).
  // We solve this by mapping the time to a year with same leap-year-ness
  // and same starting day for the year.  The ECMAscript specification says
  // we must do this, but for compatibility with other browsers, we use
  // the actual year if it is in the range 1970..2037
  if (t >= 0 && t <= 2.1e12) return t;
  var day = MakeDay(EquivalentYear(YearFromTime(t)), MonthFromTime(t), DateFromTime(t));
  return TimeClip(MakeDate(day, TimeWithinDay(t)));
}


// Because computing the DST offset is a pretty expensive operation
// we keep a cache of last computed offset along with a time interval
// where we know the cache is valid.
var DST_offset_cache = {
  // Cached DST offset.
  offset: 0,
  // Time interval where the cached offset is valid.
  start: 0, end: -1,
  // Size of next interval expansion.
  increment: 0
};


// NOTE: The implementation relies on the fact that no time zones have
// more than one daylight savings offset change per month.
function DaylightSavingsOffset(t) {
  // Load the cache object from the builtins object.
  var cache = DST_offset_cache;

  // Cache the start and the end in local variables for fast access.
  var start = cache.start;
  var end = cache.end;

  if (start <= t) {
    // If the time fits in the cached interval, return the cached offset.
    if (t <= end) return cache.offset;

    // Compute a possible new interval end.
    var new_end = end + cache.increment;

    if (t <= new_end) {
      var end_offset = %DateDaylightSavingsOffset(EquivalentTime(new_end));
      if (cache.offset == end_offset) {
        // If the offset at the end of the new interval still matches
        // the offset in the cache, we grow the cached time interval
        // and return the offset.
        cache.end = new_end;
        cache.increment = msPerMonth;
        return end_offset;
      } else {
        var offset = %DateDaylightSavingsOffset(EquivalentTime(t));
        if (offset == end_offset) {
          // The offset at the given time is equal to the offset at the
          // new end of the interval, so that means that we've just skipped
          // the point in time where the DST offset change occurred. Updated
          // the interval to reflect this and reset the increment.
          cache.start = t;
          cache.end = new_end;
          cache.increment = msPerMonth;
        } else {
          // The interval contains a DST offset change and the given time is
          // before it. Adjust the increment to avoid a linear search for
          // the offset change point and change the end of the interval.
          cache.increment /= 3;
          cache.end = t;
        }
        // Update the offset in the cache and return it.
        cache.offset = offset;
        return offset;
      }
    }
  }

  // Compute the DST offset for the time and shrink the cache interval
  // to only contain the time. This allows fast repeated DST offset
  // computations for the same time.
  var offset = %DateDaylightSavingsOffset(EquivalentTime(t));
  cache.offset = offset;
  cache.start = cache.end = t;
  cache.increment = msPerMonth;
  return offset;
}


var timezone_cache_time = $NaN;
var timezone_cache_timezone;

function LocalTimezone(t) {
  if (t == timezone_cache_time) {
    return timezone_cache_timezone;
  }
  var timezone = %DateLocalTimezone(EquivalentTime(t));
  timezone_cache_time = t;
  timezone_cache_timezone = timezone;
  return timezone;
}


function WeekDay(time) {
  return Modulo(Day(time) + 4, 7);
}

var local_time_offset = %DateLocalTimeOffset();

function LocalTime(time) {
  if (NUMBER_IS_NAN(time)) return time;
  return time + local_time_offset + DaylightSavingsOffset(time);
}

function LocalTimeNoCheck(time) {
  return time + local_time_offset + DaylightSavingsOffset(time);
}


function UTC(time) {
  if (NUMBER_IS_NAN(time)) return time;
  var tmp = time - local_time_offset;
  return tmp - DaylightSavingsOffset(tmp);
}


// ECMA 262 - 15.9.1.10
function HourFromTime(time) {
  return Modulo(FLOOR(time / msPerHour), HoursPerDay);
}


function MinFromTime(time) {
  return Modulo(FLOOR(time / msPerMinute), MinutesPerHour);
}


function SecFromTime(time) {
  return Modulo(FLOOR(time / msPerSecond), SecondsPerMinute);
}


function msFromTime(time) {
  return Modulo(time, msPerSecond);
}


// ECMA 262 - 15.9.1.11
function MakeTime(hour, min, sec, ms) {
  if (!$isFinite(hour)) return $NaN;
  if (!$isFinite(min)) return $NaN;
  if (!$isFinite(sec)) return $NaN;
  if (!$isFinite(ms)) return $NaN;
  return TO_INTEGER(hour) * msPerHour
      + TO_INTEGER(min) * msPerMinute
      + TO_INTEGER(sec) * msPerSecond
      + TO_INTEGER(ms);
}


// ECMA 262 - 15.9.1.12
function TimeInYear(year) {
  return DaysInYear(year) * msPerDay;
}


// Compute modified Julian day from year, month, date.
function ToJulianDay(year, month, date) {
  var jy = (month > 1) ? year : year - 1;
  var jm = (month > 1) ? month + 2 : month + 14;
  var ja = FLOOR(jy / 100);
  return FLOOR(FLOOR(365.25*jy) + FLOOR(30.6001*jm) + date + 1720995) + 2 - ja + FLOOR(0.25*ja);
}

var four_year_cycle_table = CalculateDateTable();


function CalculateDateTable() {
  var month_lengths = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
  var four_year_cycle_table = new $Array(1461);

  var cumulative = 0;
  var position = 0;
  var leap_position = 0;
  for (var month = 0; month < 12; month++) {
    var month_bits = month << kMonthShift;
    var length = month_lengths[month];
    for (var day = 1; day <= length; day++) {
      four_year_cycle_table[leap_position] =
        month_bits + day;
      four_year_cycle_table[366 + position] =
        (1 << kYearShift) + month_bits + day;
      four_year_cycle_table[731 + position] =
        (2 << kYearShift) + month_bits + day;
      four_year_cycle_table[1096 + position] =
        (3 << kYearShift) + month_bits + day;
      leap_position++;
      position++;
    }
    if (month == 1) {
      four_year_cycle_table[leap_position++] = month_bits + 29;
    }
  }
  return four_year_cycle_table;
}


// Constructor for creating objects holding year, month, and date.
// Introduced to ensure the two return points in FromJulianDay match same map.
function DayTriplet(year, month, date) {
  this.year = year;
  this.month = month;
  this.date = date;
}

var julian_day_cache_triplet;
var julian_day_cache_day = $NaN;

// Compute year, month, and day from modified Julian day.
// The missing days in 1582 are ignored for JavaScript compatibility.
function FromJulianDay(julian) {
  if (julian_day_cache_day == julian) {
    return julian_day_cache_triplet;
  }
  var result;
  // Avoid floating point and non-Smi maths in common case.  This is also a period of
  // time where leap years are very regular.  The range is not too large to avoid overflow
  // when doing the multiply-to-divide trick.
  if (julian > kDayZeroInJulianDay &&
      (julian - kDayZeroInJulianDay) < 40177) { // 1970 - 2080
    var jsimple = (julian - kDayZeroInJulianDay) + 731; // Day 0 is 1st January 1968
    var y = 1968;
    // Divide by 1461 by multiplying with 22967 and shifting down by 25!
    var after_1968 = (jsimple * 22967) >> 25;
    y += after_1968 << 2;
    jsimple -= 1461 * after_1968;
    var four_year_cycle = four_year_cycle_table[jsimple];
    result = new DayTriplet(y + (four_year_cycle >> kYearShift),
                            (four_year_cycle & kMonthMask) >> kMonthShift,
                            four_year_cycle & kDayMask);
  } else {
    var jalpha = FLOOR((julian - 1867216.25) / 36524.25);
    var jb = julian + 1 + jalpha - FLOOR(0.25 * jalpha) + 1524;
    var jc = FLOOR(6680.0 + ((jb-2439870) - 122.1)/365.25);
    var jd = FLOOR(365 * jc + (0.25 * jc));
    var je = FLOOR((jb - jd)/30.6001);
    var m = je - 1;
    if (m > 12) m -= 13;
    var y = jc - 4715;
    if (m > 2) { --y; --m; }
    var d = jb - jd - FLOOR(30.6001 * je);
    result = new DayTriplet(y, m, d);
  }
  julian_day_cache_day = julian;
  julian_day_cache_triplet = result;
  return result;
}


// Compute number of days given a year, month, date.
// Note that month and date can lie outside the normal range.
//   For example:
//     MakeDay(2007, -4, 20) --> MakeDay(2006, 8, 20)
//     MakeDay(2007, -33, 1) --> MakeDay(2004, 3, 1)
//     MakeDay(2007, 14, -50) --> MakeDay(2007, 8, 11)
function MakeDay(year, month, date) {
  if (!$isFinite(year) || !$isFinite(month) || !$isFinite(date)) return $NaN;

  // Conversion to integers.
  year = TO_INTEGER(year);
  month = TO_INTEGER(month);
  date = TO_INTEGER(date);

  // Overflow months into year.
  year = year + FLOOR(month/12);
  month = month % 12;
  if (month < 0) {
    month += 12;
  }

  // Return days relative to Jan 1 1970.
  return ToJulianDay(year, month, date) - kDayZeroInJulianDay;
}


// ECMA 262 - 15.9.1.13
function MakeDate(day, time) {
  if (!$isFinite(day)) return $NaN;
  if (!$isFinite(time)) return $NaN;
  return day * msPerDay + time;
}


// ECMA 262 - 15.9.1.14
function TimeClip(time) {
  if (!$isFinite(time)) return $NaN;
  if ($abs(time) > 8.64E15) return $NaN;
  return TO_INTEGER(time);
}


// The Date cache is used to limit the cost of parsing the same Date
// strings over and over again.
var Date_cache = {
  // Cached time value.
  time: $NaN,
  // Cached year when interpreting the time as a local time. Only
  // valid when the time matches cached time.
  year: $NaN,
  // String input for which the cached time is valid.
  string: null
};


%SetCode($Date, function(year, month, date, hours, minutes, seconds, ms) {
  if (!%_IsConstructCall()) {
    // ECMA 262 - 15.9.2
    return (new $Date()).toString();
  }

  // ECMA 262 - 15.9.3
  var argc = %_ArgumentsLength();
  var value;
  if (argc == 0) {
    value = %DateCurrentTime();

  } else if (argc == 1) {
    if (IS_NUMBER(year)) {
      value = TimeClip(year);

    } else if (IS_STRING(year)) {
      // Probe the Date cache. If we already have a time value for the
      // given time, we re-use that instead of parsing the string again.
      var cache = Date_cache;
      if (cache.string === year) {
        value = cache.time;
      } else {
        value = DateParse(year);
        cache.time = value;
        cache.year = YearFromTime(LocalTimeNoCheck(value));
        cache.string = year;
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
      value = IS_STRING(time) ? DateParse(time) : TimeClip(ToNumber(time));
    }

  } else {
    year = ToNumber(year);
    month = ToNumber(month);
    date = argc > 2 ? ToNumber(date) : 1;
    hours = argc > 3 ? ToNumber(hours) : 0;
    minutes = argc > 4 ? ToNumber(minutes) : 0;
    seconds = argc > 5 ? ToNumber(seconds) : 0;
    ms = argc > 6 ? ToNumber(ms) : 0;
    year = (!NUMBER_IS_NAN(year) && 0 <= TO_INTEGER(year) && TO_INTEGER(year) <= 99)
        ? 1900 + TO_INTEGER(year) : year;
    var day = MakeDay(year, month, date);
    var time = MakeTime(hours, minutes, seconds, ms);
    value = TimeClip(UTC(MakeDate(day, time)));
  }
  %_SetValueOf(this, value);
});


// Helper functions.
function GetTimeFrom(aDate) {
  return DATE_VALUE(aDate);
}


function GetMillisecondsFrom(aDate) {
  var t = DATE_VALUE(aDate);
  if (NUMBER_IS_NAN(t)) return t;
  return msFromTime(LocalTimeNoCheck(t));
}


function GetUTCMillisecondsFrom(aDate) {
  var t = DATE_VALUE(aDate);
  if (NUMBER_IS_NAN(t)) return t;
  return msFromTime(t);
}


function GetSecondsFrom(aDate) {
  var t = DATE_VALUE(aDate);
  if (NUMBER_IS_NAN(t)) return t;
  return SecFromTime(LocalTimeNoCheck(t));
}


function GetUTCSecondsFrom(aDate) {
  var t = DATE_VALUE(aDate);
  if (NUMBER_IS_NAN(t)) return t;
  return SecFromTime(t);
}


function GetMinutesFrom(aDate) {
  var t = DATE_VALUE(aDate);
  if (NUMBER_IS_NAN(t)) return t;
  return MinFromTime(LocalTimeNoCheck(t));
}


function GetUTCMinutesFrom(aDate) {
  var t = DATE_VALUE(aDate);
  if (NUMBER_IS_NAN(t)) return t;
  return MinFromTime(t);
}


function GetHoursFrom(aDate) {
  var t = DATE_VALUE(aDate);
  if (NUMBER_IS_NAN(t)) return t;
  return HourFromTime(LocalTimeNoCheck(t));
}


function GetUTCHoursFrom(aDate) {
  var t = DATE_VALUE(aDate);
  if (NUMBER_IS_NAN(t)) return t;
  return HourFromTime(t);
}


function GetFullYearFrom(aDate) {
  var t = DATE_VALUE(aDate);
  if (NUMBER_IS_NAN(t)) return t;
  var cache = Date_cache;
  if (cache.time === t) return cache.year;
  return YearFromTime(LocalTimeNoCheck(t));
}


function GetUTCFullYearFrom(aDate) {
  var t = DATE_VALUE(aDate);
  if (NUMBER_IS_NAN(t)) return t;
  return YearFromTime(t);
}


function GetMonthFrom(aDate) {
  var t = DATE_VALUE(aDate);
  if (NUMBER_IS_NAN(t)) return t;
  return MonthFromTime(LocalTimeNoCheck(t));
}


function GetUTCMonthFrom(aDate) {
  var t = DATE_VALUE(aDate);
  if (NUMBER_IS_NAN(t)) return t;
  return MonthFromTime(t);
}


function GetDateFrom(aDate) {
  var t = DATE_VALUE(aDate);
  if (NUMBER_IS_NAN(t)) return t;
  return DateFromTime(LocalTimeNoCheck(t));
}


function GetUTCDateFrom(aDate) {
  var t = DATE_VALUE(aDate);
  if (NUMBER_IS_NAN(t)) return t;
  return DateFromTime(t);
}


%FunctionSetPrototype($Date, new $Date($NaN));


var WeekDays = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
var Months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];


function TwoDigitString(value) {
  return value < 10 ? "0" + value : "" + value;
}


function DateString(time) {
  var YMD = FromJulianDay(Day(time) + kDayZeroInJulianDay);
  return WeekDays[WeekDay(time)] + ' '
      + Months[YMD.month] + ' '
      + TwoDigitString(YMD.date) + ' '
      + YMD.year;
}


var LongWeekDays = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
var LongMonths = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];


function LongDateString(time) {
  var YMD = FromJulianDay(Day(time) + kDayZeroInJulianDay);
  return LongWeekDays[WeekDay(time)] + ', '
      + LongMonths[YMD.month] + ' '
      + TwoDigitString(YMD.date) + ', '
      + YMD.year;
}


function TimeString(time) {
  return TwoDigitString(HourFromTime(time)) + ':'
      + TwoDigitString(MinFromTime(time)) + ':'
      + TwoDigitString(SecFromTime(time));
}


function LocalTimezoneString(time) {
  var timezoneOffset = (local_time_offset + DaylightSavingsOffset(time)) / msPerMinute;
  var sign = (timezoneOffset >= 0) ? 1 : -1;
  var hours = FLOOR((sign * timezoneOffset)/60);
  var min   = FLOOR((sign * timezoneOffset)%60);
  var gmt = ' GMT' + ((sign == 1) ? '+' : '-') + TwoDigitString(hours) + TwoDigitString(min);
  return gmt + ' (' +  LocalTimezone(time) + ')';
}


function DatePrintString(time) {
  return DateString(time) + ' ' + TimeString(time);
}

// -------------------------------------------------------------------

// Reused output buffer. Used when parsing date strings.
var parse_buffer = $Array(7);

// ECMA 262 - 15.9.4.2
function DateParse(string) {
  var arr = %DateParseString(ToString(string), parse_buffer);
  if (IS_NULL(arr)) return $NaN;

  var day = MakeDay(arr[0], arr[1], arr[2]);
  var time = MakeTime(arr[3], arr[4], arr[5], 0);
  var date = MakeDate(day, time);

  if (IS_NULL(arr[6])) {
    return TimeClip(UTC(date));
  } else {
    return TimeClip(date - arr[6] * 1000);
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
  year = (!NUMBER_IS_NAN(year) && 0 <= TO_INTEGER(year) && TO_INTEGER(year) <= 99)
      ? 1900 + TO_INTEGER(year) : year;
  var day = MakeDay(year, month, date);
  var time = MakeTime(hours, minutes, seconds, ms);
  return %_SetValueOf(this, TimeClip(MakeDate(day, time)));
}


// Mozilla-specific extension. Returns the number of milliseconds
// elapsed since 1 January 1970 00:00:00 UTC.
function DateNow() {
  return %DateCurrentTime();
}


// ECMA 262 - 15.9.5.2
function DateToString() {
  var t = DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  return DatePrintString(LocalTimeNoCheck(t)) + LocalTimezoneString(t);
}


// ECMA 262 - 15.9.5.3
function DateToDateString() {
  var t = DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  return DateString(LocalTimeNoCheck(t));
}


// ECMA 262 - 15.9.5.4
function DateToTimeString() {
  var t = DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  var lt = LocalTimeNoCheck(t);
  return TimeString(lt) + LocalTimezoneString(lt);
}


// ECMA 262 - 15.9.5.5
function DateToLocaleString() {
  return DateToString.call(this);
}


// ECMA 262 - 15.9.5.6
function DateToLocaleDateString() {
  var t = DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  return LongDateString(LocalTimeNoCheck(t));
}


// ECMA 262 - 15.9.5.7
function DateToLocaleTimeString() {
  var t = DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  var lt = LocalTimeNoCheck(t);
  return TimeString(lt);
}


// ECMA 262 - 15.9.5.8
function DateValueOf() {
  return DATE_VALUE(this);
}


// ECMA 262 - 15.9.5.9
function DateGetTime() {
  return DATE_VALUE(this);
}


// ECMA 262 - 15.9.5.10
function DateGetFullYear() {
  return GetFullYearFrom(this)
}


// ECMA 262 - 15.9.5.11
function DateGetUTCFullYear() {
  return GetUTCFullYearFrom(this)
}


// ECMA 262 - 15.9.5.12
function DateGetMonth() {
  return GetMonthFrom(this);
}


// ECMA 262 - 15.9.5.13
function DateGetUTCMonth() {
  return GetUTCMonthFrom(this);
}


// ECMA 262 - 15.9.5.14
function DateGetDate() {
  return GetDateFrom(this);
}


// ECMA 262 - 15.9.5.15
function DateGetUTCDate() {
  return GetUTCDateFrom(this);
}


// ECMA 262 - 15.9.5.16
function DateGetDay() {
  var t = %_ValueOf(this);
  if (NUMBER_IS_NAN(t)) return t;
  return WeekDay(LocalTimeNoCheck(t));
}


// ECMA 262 - 15.9.5.17
function DateGetUTCDay() {
  var t = %_ValueOf(this);
  if (NUMBER_IS_NAN(t)) return t;
  return WeekDay(t);
}


// ECMA 262 - 15.9.5.18
function DateGetHours() {
  return GetHoursFrom(this);
}


// ECMA 262 - 15.9.5.19
function DateGetUTCHours() {
  return GetUTCHoursFrom(this);
}


// ECMA 262 - 15.9.5.20
function DateGetMinutes() {
  return GetMinutesFrom(this);
}


// ECMA 262 - 15.9.5.21
function DateGetUTCMinutes() {
  return GetUTCMinutesFrom(this);
}


// ECMA 262 - 15.9.5.22
function DateGetSeconds() {
  return GetSecondsFrom(this);
}


// ECMA 262 - 15.9.5.23
function DateGetUTCSeconds() {
  return GetUTCSecondsFrom(this);
}


// ECMA 262 - 15.9.5.24
function DateGetMilliseconds() {
  return GetMillisecondsFrom(this);
}


// ECMA 262 - 15.9.5.25
function DateGetUTCMilliseconds() {
  return GetUTCMillisecondsFrom(this);
}


// ECMA 262 - 15.9.5.26
function DateGetTimezoneOffset() {
  var t = DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return t;
  return (t - LocalTimeNoCheck(t)) / msPerMinute;
}


// ECMA 262 - 15.9.5.27
function DateSetTime(ms) {
  if (!IS_DATE(this)) ThrowDateTypeError();
  return %_SetValueOf(this, TimeClip(ToNumber(ms)));
}


// ECMA 262 - 15.9.5.28
function DateSetMilliseconds(ms) {
  var t = LocalTime(DATE_VALUE(this));
  ms = ToNumber(ms);
  var time = MakeTime(HourFromTime(t), MinFromTime(t), SecFromTime(t), ms);
  return %_SetValueOf(this, TimeClip(UTC(MakeDate(Day(t), time))));
}


// ECMA 262 - 15.9.5.29
function DateSetUTCMilliseconds(ms) {
  var t = DATE_VALUE(this);
  ms = ToNumber(ms);
  var time = MakeTime(HourFromTime(t), MinFromTime(t), SecFromTime(t), ms);
  return %_SetValueOf(this, TimeClip(MakeDate(Day(t), time)));
}


// ECMA 262 - 15.9.5.30
function DateSetSeconds(sec, ms) {
  var t = LocalTime(DATE_VALUE(this));
  sec = ToNumber(sec);
  ms = %_ArgumentsLength() < 2 ? GetMillisecondsFrom(this) : ToNumber(ms);
  var time = MakeTime(HourFromTime(t), MinFromTime(t), sec, ms);
  return %_SetValueOf(this, TimeClip(UTC(MakeDate(Day(t), time))));
}


// ECMA 262 - 15.9.5.31
function DateSetUTCSeconds(sec, ms) {
  var t = DATE_VALUE(this);
  sec = ToNumber(sec);
  ms = %_ArgumentsLength() < 2 ? GetUTCMillisecondsFrom(this) : ToNumber(ms);
  var time = MakeTime(HourFromTime(t), MinFromTime(t), sec, ms);
  return %_SetValueOf(this, TimeClip(MakeDate(Day(t), time)));
}


// ECMA 262 - 15.9.5.33
function DateSetMinutes(min, sec, ms) {
  var t = LocalTime(DATE_VALUE(this));
  min = ToNumber(min);
  var argc = %_ArgumentsLength();
  sec = argc < 2 ? GetSecondsFrom(this) : ToNumber(sec);
  ms = argc < 3 ? GetMillisecondsFrom(this) : ToNumber(ms);
  var time = MakeTime(HourFromTime(t), min, sec, ms);
  return %_SetValueOf(this, TimeClip(UTC(MakeDate(Day(t), time))));
}


// ECMA 262 - 15.9.5.34
function DateSetUTCMinutes(min, sec, ms) {
  var t = DATE_VALUE(this);
  min = ToNumber(min);
  var argc = %_ArgumentsLength();
  sec = argc < 2 ? GetUTCSecondsFrom(this) : ToNumber(sec);
  ms = argc < 3 ? GetUTCMillisecondsFrom(this) : ToNumber(ms);
  var time = MakeTime(HourFromTime(t), min, sec, ms);
  return %_SetValueOf(this, TimeClip(MakeDate(Day(t), time)));
}


// ECMA 262 - 15.9.5.35
function DateSetHours(hour, min, sec, ms) {
  var t = LocalTime(DATE_VALUE(this));
  hour = ToNumber(hour);
  var argc = %_ArgumentsLength();
  min = argc < 2 ? GetMinutesFrom(this) : ToNumber(min);
  sec = argc < 3 ? GetSecondsFrom(this) : ToNumber(sec);
  ms = argc < 4 ? GetMillisecondsFrom(this) : ToNumber(ms);
  var time = MakeTime(hour, min, sec, ms);
  return %_SetValueOf(this, TimeClip(UTC(MakeDate(Day(t), time))));
}


// ECMA 262 - 15.9.5.34
function DateSetUTCHours(hour, min, sec, ms) {
  var t = DATE_VALUE(this);
  hour = ToNumber(hour);
  var argc = %_ArgumentsLength();
  min = argc < 2 ? GetUTCMinutesFrom(this) : ToNumber(min);
  sec = argc < 3 ? GetUTCSecondsFrom(this) : ToNumber(sec);
  ms = argc < 4 ? GetUTCMillisecondsFrom(this) : ToNumber(ms);
  var time = MakeTime(hour, min, sec, ms);
  return %_SetValueOf(this, TimeClip(MakeDate(Day(t), time)));
}


// ECMA 262 - 15.9.5.36
function DateSetDate(date) {
  var t = LocalTime(DATE_VALUE(this));
  date = ToNumber(date);
  var day = MakeDay(YearFromTime(t), MonthFromTime(t), date);
  return %_SetValueOf(this, TimeClip(UTC(MakeDate(day, TimeWithinDay(t)))));
}


// ECMA 262 - 15.9.5.37
function DateSetUTCDate(date) {
  var t = DATE_VALUE(this);
  date = ToNumber(date);
  var day = MakeDay(YearFromTime(t), MonthFromTime(t), date);
  return %_SetValueOf(this, TimeClip(MakeDate(day, TimeWithinDay(t))));
}


// ECMA 262 - 15.9.5.38
function DateSetMonth(month, date) {
  var t = LocalTime(DATE_VALUE(this));
  month = ToNumber(month);
  date = %_ArgumentsLength() < 2 ? GetDateFrom(this) : ToNumber(date);
  var day = MakeDay(YearFromTime(t), month, date);
  return %_SetValueOf(this, TimeClip(UTC(MakeDate(day, TimeWithinDay(t)))));
}


// ECMA 262 - 15.9.5.39
function DateSetUTCMonth(month, date) {
  var t = DATE_VALUE(this);
  month = ToNumber(month);
  date = %_ArgumentsLength() < 2 ? GetUTCDateFrom(this) : ToNumber(date);
  var day = MakeDay(YearFromTime(t), month, date);
  return %_SetValueOf(this, TimeClip(MakeDate(day, TimeWithinDay(t))));
}


// ECMA 262 - 15.9.5.40
function DateSetFullYear(year, month, date) {
  var t = DATE_VALUE(this);
  t = NUMBER_IS_NAN(t) ? 0 : LocalTimeNoCheck(t);
  year = ToNumber(year);
  var argc = %_ArgumentsLength();
  month = argc < 2 ? MonthFromTime(t) : ToNumber(month);
  date = argc < 3 ? DateFromTime(t) : ToNumber(date);
  var day = MakeDay(year, month, date);
  return %_SetValueOf(this, TimeClip(UTC(MakeDate(day, TimeWithinDay(t)))));
}


// ECMA 262 - 15.9.5.41
function DateSetUTCFullYear(year, month, date) {
  var t = DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) t = 0;
  var argc = %_ArgumentsLength();
  year = ToNumber(year);
  month = argc < 2 ? MonthFromTime(t) : ToNumber(month);
  date = argc < 3 ? DateFromTime(t) : ToNumber(date);
  var day = MakeDay(year, month, date);
  return %_SetValueOf(this, TimeClip(MakeDate(day, TimeWithinDay(t))));
}


// ECMA 262 - 15.9.5.42
function DateToUTCString() {
  var t = DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  // Return UTC string of the form: Sat, 31 Jan 1970 23:00:00 GMT
  return WeekDays[WeekDay(t)] + ', '
      + TwoDigitString(DateFromTime(t)) + ' '
      + Months[MonthFromTime(t)] + ' '
      + YearFromTime(t) + ' '
      + TimeString(t) + ' GMT';
}


// ECMA 262 - B.2.4
function DateGetYear() {
  var t = DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return $NaN;
  return YearFromTime(LocalTimeNoCheck(t)) - 1900;
}


// ECMA 262 - B.2.5
function DateSetYear(year) {
  var t = LocalTime(DATE_VALUE(this));
  if (NUMBER_IS_NAN(t)) t = 0;
  year = ToNumber(year);
  if (NUMBER_IS_NAN(year)) return %_SetValueOf(this, $NaN);
  year = (0 <= TO_INTEGER(year) && TO_INTEGER(year) <= 99)
      ? 1900 + TO_INTEGER(year) : year;
  var day = MakeDay(year, MonthFromTime(t), DateFromTime(t));
  return %_SetValueOf(this, TimeClip(UTC(MakeDate(day, TimeWithinDay(t)))));
}


// ECMA 262 - B.2.6
//
// Notice that this does not follow ECMA 262 completely.  ECMA 262
// says that toGMTString should be the same Function object as
// toUTCString.  JSC does not do this, so for compatibility we do not
// do that either.  Instead, we create a new function whose name
// property will return toGMTString.
function DateToGMTString() {
  return DateToUTCString.call(this);
}


function PadInt(n) {
  // Format integers to have at least two digits.
  return n < 10 ? '0' + n : n;
}


function DateToISOString() {
  return this.getUTCFullYear() + '-' + PadInt(this.getUTCMonth() + 1) +
      '-' + PadInt(this.getUTCDate()) + 'T' + PadInt(this.getUTCHours()) +
      ':' + PadInt(this.getUTCMinutes()) + ':' + PadInt(this.getUTCSeconds()) +
      'Z';
}


function DateToJSON(key) {
  return CheckJSONPrimitive(this.toISOString());
}


// -------------------------------------------------------------------

function SetupDate() {
  // Setup non-enumerable properties of the Date object itself.
  InstallFunctions($Date, DONT_ENUM, $Array(
    "UTC", DateUTC,
    "parse", DateParse,
    "now", DateNow
  ));

  // Setup non-enumerable constructor property of the Date prototype object.
  %SetProperty($Date.prototype, "constructor", $Date, DONT_ENUM);

  // Setup non-enumerable functions of the Date prototype object and
  // set their names.
  InstallFunctionsOnHiddenPrototype($Date.prototype, DONT_ENUM, $Array(
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

SetupDate();
