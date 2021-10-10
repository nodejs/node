// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper function to test Tempral

function assertDuration(duration, years, months, weeks, days, hours,
    minutes, seconds, milliseconds, microseconds, nanoseconds, sign, blank) {
  assertEquals(years, duration.years, duration);
  assertEquals(months, duration.months, duration);
  assertEquals(weeks, duration.weeks, duration);
  assertEquals(days, duration.days, duration);
  assertEquals(hours, duration.hours, duration);
  assertEquals(minutes, duration.minutes, duration);
  assertEquals(seconds, duration.seconds, duration);
  assertEquals(milliseconds, duration.milliseconds, duration);
  assertEquals(microseconds, duration.microseconds, duration);
  assertEquals(nanoseconds, duration.nanoseconds, duration);
  assertEquals(sign, duration.sign, duration);
  assertEquals(blank, duration.blank, duration);
}

function assertPlainDate(time, year, month, day) {
  let fields = time.getISOFields();
  let keys = Object.keys(fields);
  assertEquals(4, keys.length);
  assertEquals("calendar", keys[0]);
  assertEquals("isoDay", keys[1]);
  assertEquals("isoMonth", keys[2]);
  assertEquals("isoYear", keys[3]);
  if (fields.calendar == "iso8601") {
    assertEquals(year, fields.isoYear, "isoYear");
    assertEquals(month, fields.isoMonth, "isoMonth");
    assertEquals(day, fields.isoDay, "isoDay");
  }
}

function assertPlainDateTime(datetime, year, month, day, hour, minute, second,
    millisecond, microsecond, nanosecond) {
  let fields = datetime.getISOFields();
  let keys = Object.keys(fields);
  assertEquals(10, keys.length);
  assertEquals("calendar", keys[0]);
  assertEquals("isoDay", keys[1]);
  assertEquals("isoHour", keys[2]);
  assertEquals("isoMicrosecond", keys[3]);
  assertEquals("isoMillisecond", keys[4]);
  assertEquals("isoMinute", keys[5]);
  assertEquals("isoMonth", keys[6]);
  assertEquals("isoNanosecond", keys[7]);
  assertEquals("isoSecond", keys[8]);
  assertEquals("isoYear", keys[9]);
  if (fields.calendar == "iso8601") {
    assertEquals(year, fields.isoYear, "isoYear");
    assertEquals(month, fields.isoMonth, "isoMonth");
    assertEquals(day, fields.isoDay, "isoDay");
    assertEquals(hour, fields.isoHour, "isoHour");
    assertEquals(minute, fields.isoMinute, "isoMinute");
    assertEquals(second, fields.isoSecond, "isoSecond");
    assertEquals(millisecond, fields.isoMillisecond, "isoMillisecond");
    assertEquals(microsecond, fields.isoMicrosecond, "isoMicorsecond");
    assertEquals(nanosecond, fields.isoNanosecond, "isoNanosecond");
    assertEquals(datetime.calendar, fields.calendar, "calendar");
  }
}

function assertPlainTime(time, hour, minute, second, millisecond, microsecond, nanosecond) {
  assertEquals(hour, time.hour, "hour");
  assertEquals(minute, time.minute, "minute");
  assertEquals(second, time.second, "second");
  assertEquals(millisecond, time.millisecond, "millisecond");
  assertEquals(microsecond, time.microsecond, "microsecond");
  assertEquals(nanosecond, time.nanosecond, "nanosecond");
}

function assertPlainMonthDay(md, monthCode, day) {
  let fields = md.getISOFields();
  let keys = Object.keys(fields);
  assertEquals(4, keys.length);
  assertEquals("calendar", keys[0]);
  assertEquals("isoDay", keys[1]);
  assertEquals("isoMonth", keys[2]);
  assertEquals("isoYear", keys[3]);
  assertEquals(monthCode, md.monthCode, "monthCode");
  assertEquals(day, md.day, "day");

  if (fields.calendar == "iso8601") {
    assertEquals(monthCode, md.monthCode, "monthCode");
    assertEquals(day, md.day, "day");
    assertEquals(md.calendar, fields.calendar, "calendar");
  }
}

function assertPlainYearMonth(ym, year, month) {
  let fields = ym.getISOFields();
  let keys = Object.keys(fields);
  assertEquals(4, keys.length);
  assertEquals("calendar", keys[0]);
  assertEquals("isoDay", keys[1]);
  assertEquals("isoMonth", keys[2]);
  assertEquals("isoYear", keys[3]);
  if (fields.calendar == "iso8601") {
    assertEquals(year, fields.isoYear, "isoYear");
    assertEquals(month, fields.isoMonth, "isoMonth");
    assertEquals(ym.calendar, fields.calendar, "calendar");
  }
}
