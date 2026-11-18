// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Ensure ISO year in PlainMonthDay.from, PlainMonthDay.prototype.with, and
// PlainMonthDay.prototype.equals is only used to determine leap-year status for
// overflow handling, even when outside int32 range or above MAX_SAFE_INTEGER.

function assertMonthDay(md, monthCode, day) {
  assertEquals(monthCode, md.monthCode);
  assertEquals(day, md.day);
  assertEquals('1972', md.toString({calendarName: 'always'}).split('-')[0]);
}

// Years at/outside int32 boundaries and above 2**53:
// - 2147483647 (INT32_MAX), -2147483649 (INT32_MIN - 1): odd -> common years.
// - 1000000000000005, -1000000000000005: common years (previously clamped to
//   INT32_MAX = 2147483647 [common] and INT32_MIN = -2147483648 [leap!]).
// - 9007199254740994 (2**53 + 2): even, not divisible by 4 -> common year.
// - +-18014398509482100 (+-(2**54 + 116)): divisible by 100, not 400 -> common
//   years (negative value exercises fmod -> -100 -> +300).
const extremeCommonYears = [
  -999999,
  -2147483649,
  2147483647,
  1000000000000005,
  -1000000000000005,
  9007199254740994,
  18014398509482100,
  -18014398509482100,
];

// - -0: 0 is divisible by 400 -> leap year.
// - -2147483648 (INT32_MIN), 2147483648 (INT32_MAX + 1): divisible by 4, not
//   100 -> leap years.
// - 1000000000000004, -1000000000000004: leap years (1000000000000004
//   previously clamped to INT32_MAX = 2147483647 [common!]).
// - 9007199254740992 (2**53): divisible by 4, not 100 -> leap year.
// - 18014398509482000 (2**54 + 16): divisible by 400 -> leap year.
// - Number.MAX_VALUE / -Number.MAX_VALUE: === +-368 (mod 400) -> leap years.
const extremeLeapYears = [
  -0,
  -1000000,
  -2147483648,
  2147483648,
  1000000000000004,
  -1000000000000004,
  9007199254740992,
  18014398509482000,
  Number.MAX_VALUE,
  -Number.MAX_VALUE,
];

const jan1 = new Temporal.PlainMonthDay(1, 1);
const feb28 = new Temporal.PlainMonthDay(2, 28);
const feb29 = new Temporal.PlainMonthDay(2, 29);

for (const year of extremeCommonYears) {
  assertMonthDay(
      Temporal.PlainMonthDay.from({year, month: 1, day: 1}), 'M01', 1);
  assertMonthDay(
      Temporal.PlainMonthDay.from(
          {year, month: 1, day: 1}, {overflow: 'reject'}),
      'M01', 1);
  assertMonthDay(
      Temporal.PlainMonthDay.from({year, monthCode: 'M02', day: 29}), 'M02',
      28);
  assertThrows(
      () => Temporal.PlainMonthDay.from(
          {year, monthCode: 'M02', day: 29}, {overflow: 'reject'}),
      RangeError);

  assertMonthDay(jan1.with({year}), 'M01', 1);
  assertMonthDay(jan1.with({year}, {overflow: 'reject'}), 'M01', 1);
  assertMonthDay(feb29.with({year}), 'M02', 28);
  assertThrows(() => feb29.with({year}, {overflow: 'reject'}), RangeError);

  assertTrue(feb28.equals({year, monthCode: 'M02', day: 29}));
  assertFalse(feb29.equals({year, monthCode: 'M02', day: 29}));
}

for (const year of extremeLeapYears) {
  assertMonthDay(
      Temporal.PlainMonthDay.from({year, month: 1, day: 1}), 'M01', 1);
  assertMonthDay(
      Temporal.PlainMonthDay.from(
          {year, month: 1, day: 1}, {overflow: 'reject'}),
      'M01', 1);
  assertMonthDay(
      Temporal.PlainMonthDay.from({year, monthCode: 'M02', day: 29}), 'M02',
      29);
  assertMonthDay(
      Temporal.PlainMonthDay.from(
          {year, monthCode: 'M02', day: 29}, {overflow: 'reject'}),
      'M02', 29);

  assertMonthDay(jan1.with({year}), 'M01', 1);
  assertMonthDay(feb29.with({year}), 'M02', 29);
  assertMonthDay(feb29.with({year}, {overflow: 'reject'}), 'M02', 29);

  assertTrue(feb29.equals({year, monthCode: 'M02', day: 29}));
}

// Missing required fields with an extreme ISO year and overflow: "reject" must
// still throw TypeError from CalendarResolveFields, not RangeError.
assertThrows(
    () => Temporal.PlainMonthDay.from(
        {year: 1000000000000005}, {overflow: 'reject'}),
    TypeError);
assertThrows(
    () => Temporal.PlainMonthDay.from(
        {year: 1000000000000005, monthCode: 'M02'}, {overflow: 'reject'}),
    TypeError);

// Non-ISO calendars still range-check the year field.
const gregoryFeb28 = new Temporal.PlainMonthDay(2, 28, 'gregory', 1972);
assertThrows(
    () => Temporal.PlainMonthDay.from({
      calendar: 'gregory',
      year: 1000000000000004,
      monthCode: 'M02',
      day: 29
    }),
    RangeError);
assertThrows(
    () => Temporal.PlainMonthDay.from(
        {
          calendar: 'gregory',
          year: 1000000000000004,
          monthCode: 'M02',
          day: 29
        },
        {overflow: 'reject'}),
    RangeError);
assertThrows(() => gregoryFeb28.with({year: 1000000000000004}), RangeError);
assertThrows(
    () => gregoryFeb28.with({year: 1000000000000004}, {overflow: 'reject'}),
    RangeError);
