// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

let d1 = Temporal.Now.plainDateISO();
// 1. Set options to ? GetOptionsObject(options).
[true, false, "string is invalid", Symbol(),
    123, 456n, Infinity, NaN, null].forEach(function(invalidOptions) {

  assertThrows(() => Temporal.PlainDate.from( d1, invalidOptions), TypeError);
    });

// a. Perform ? ToTemporalOverflow(options).
assertThrows(() => Temporal.PlainDate.from(
  d1, {overflow: "invalid overflow"}), RangeError);

[undefined, {}, {overflow: "constrain"}, {overflow: "reject"}].forEach(
    function(validOptions) {
  let d = new Temporal.PlainDate(1, 2, 3);
  let d2 = Temporal.PlainDate.from(d, validOptions);
  assertEquals(1, d2.year);
  assertEquals(2, d2.month);
  assertEquals(3, d2.day);
  assertEquals("iso8601", d2.calendar.id);
});

[undefined, {}, {overflow: "constrain"}, {overflow: "reject"}].forEach(
    function(validOptions) {
  let d3 = Temporal.PlainDate.from( {year:9, month: 8, day:7}, validOptions);
  assertEquals(9, d3.year);
  assertEquals(8, d3.month);
  assertEquals("M08", d3.monthCode);
  assertEquals(7, d3.day);
  assertEquals("iso8601", d3.calendar.id);
});

[undefined, {}, {overflow: "constrain"}].forEach(
    function(validOptions) {
  let d4 = Temporal.PlainDate.from( {year:9, month: 14, day:32}, validOptions);
  assertEquals(9, d4.year);
  assertEquals(12, d4.month);
  assertEquals("M12", d4.monthCode);
  assertEquals(31, d4.day);
  assertEquals("iso8601", d4.calendar.id);
});

assertThrows(() => Temporal.PlainDate.from(
      {year:9, month: 14, day:30}, {overflow: "reject"}), RangeError);
assertThrows(() => Temporal.PlainDate.from(
      {year:9, month: 12, day:32}, {overflow: "reject"}), RangeError);
