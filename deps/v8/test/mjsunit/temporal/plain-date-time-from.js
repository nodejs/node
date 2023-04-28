// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

let d1 = Temporal.Now.plainDateTimeISO();
// 1. Set options to ? GetOptionsObject(options).
[true, false, "string is invalid", Symbol(),
    123, 456n, Infinity, NaN, null].forEach(function(invalidOptions) {

  assertThrows(() => Temporal.PlainDateTime.from(
      d1, invalidOptions), TypeError);
    });

// a. Perform ? ToTemporalOverflow(options).
assertThrows(() => Temporal.PlainDateTime.from(
  d1, {overflow: "invalid overflow"}), RangeError);

[undefined, {}, {overflow: "constrain"}, {overflow: "reject"}].forEach(
    function(validOptions) {
  let d = new Temporal.PlainDateTime(1, 2, 3, 4, 5, 6, 7, 8, 9);
  let d2 = Temporal.PlainDateTime.from(d, validOptions);
  assertEquals(1, d2.year);
  assertEquals(2, d2.month);
  assertEquals("M02", d2.monthCode);
  assertEquals(3, d2.day);
  assertEquals(4, d2.hour);
  assertEquals(5, d2.minute);
  assertEquals(6, d2.second);
  assertEquals(7, d2.millisecond);
  assertEquals(8, d2.microsecond);
  assertEquals(9, d2.nanosecond);
  assertEquals("iso8601", d2.calendar.id);
});

[undefined, {}, {overflow: "constrain"}, {overflow: "reject"}].forEach(
    function(validOptions) {
  let d3 = Temporal.PlainDateTime.from(
      {year:9, month: 8, day:7, hour: 6, minute: 5, second: 4,
        millisecond: 3, microsecond: 2, nanosecond: 1},
      validOptions);
  assertEquals(9, d3.year);
  assertEquals(8, d3.month);
  assertEquals("M08", d3.monthCode);
  assertEquals(7, d3.day);
  assertEquals(6, d3.hour);
  assertEquals(5, d3.minute);
  assertEquals(4, d3.second);
  assertEquals(3, d3.millisecond);
  assertEquals(2, d3.microsecond);
  assertEquals(1, d3.nanosecond);
  assertEquals("iso8601", d3.calendar.id);
});

[undefined, {}, {overflow: "constrain"}].forEach(
    function(validOptions) {
  let d4 = Temporal.PlainDateTime.from(
      {year:9, month: 14, day:32, hour:24, minute:60, second:60,
        millisecond: 1000, microsecond: 1000, nanosecond: 1000},
      validOptions);
  assertEquals(9, d4.year);
  assertEquals(12, d4.month);
  assertEquals("M12", d4.monthCode);
  assertEquals(31, d4.day);
  assertEquals(23, d4.hour);
  assertEquals(59, d4.minute);
  assertEquals(59, d4.second);
  assertEquals(999, d4.millisecond);
  assertEquals(999, d4.microsecond);
  assertEquals(999, d4.nanosecond);
  assertEquals("iso8601", d4.calendar.id);
});

assertThrows(() => Temporal.PlainDateTime.from(
      {year:9, month: 14, day:30}, {overflow: "reject"}), RangeError);

assertThrows(() => Temporal.PlainDateTime.from(
      {year:9, month: 12, day:32}, {overflow: "reject"}), RangeError);

assertThrows(() => Temporal.PlainDateTime.from(
      {year:9, month: 12, day:31, hour: 24}, {overflow: "reject"}), RangeError);

assertThrows(() => Temporal.PlainDateTime.from(
      {year:9, month: 12, day:31, minute: 60}, {overflow: "reject"}),
    RangeError);

assertThrows(() => Temporal.PlainDateTime.from(
      {year:9, month: 12, day:31, second: 60}, {overflow: "reject"}),
    RangeError);

assertThrows(() => Temporal.PlainDateTime.from(
      {year:9, month: 12, day:31, millisecond: 1000}, {overflow: "reject"}),
    RangeError);

assertThrows(() => Temporal.PlainDateTime.from(
      {year:9, month: 12, day:31, microsecond: 1000}, {overflow: "reject"}),
    RangeError);

assertThrows(() => Temporal.PlainDateTime.from(
      {year:9, month: 12, day:31, nanosecond: 1000}, {overflow: "reject"}),
    RangeError);
