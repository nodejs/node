// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

let like1 = {years:9, months:8, weeks:7, days:6, hours: 5, minutes: 4,
  seconds: 3, milliseconds: 2, microseconds: 1, nanoseconds: 10};
let like2 = {years: 9, hours:5};
let like3 = {months: 8, minutes:4};
let like4 = {weeks: 7, seconds:3};
let like5 = {days: 6, milliseconds:2};
let like6 = {microseconds: 987, nanoseconds: 123};
let like7 = {years:-9, months:-8, weeks:-7, days:-6, hours: -5, minutes: -4,
  seconds: -3, milliseconds: -2, microseconds: -1, nanoseconds: -10};

let d1 = new Temporal.Duration();
assertDuration(d1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, true);
assertDuration(d1.with(like1), 9, 8, 7, 6, 5, 4, 3, 2, 1, 10, 1, false);
assertDuration(d1.with(like2), 9, 0, 0, 0, 5, 0, 0, 0, 0, 0, 1, false);
assertDuration(d1.with(like3), 0, 8, 0, 0, 0, 4, 0, 0, 0, 0, 1, false);
assertDuration(d1.with(like4), 0, 0, 7, 0, 0, 0, 3, 0, 0, 0, 1, false);
assertDuration(d1.with(like5), 0, 0, 0, 6, 0, 0, 0, 2, 0, 0, 1, false);
assertDuration(d1.with(like6), 0, 0, 0, 0, 0, 0, 0, 0, 987, 123, 1, false);
assertDuration(d1.with(like7), -9, -8, -7, -6, -5, -4, -3, -2, -1, -10, -1,
    false);

let d2 = new Temporal.Duration(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
assertDuration(d2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1, false);
assertDuration(d2.with(like1), 9, 8, 7, 6, 5, 4, 3, 2, 1, 10, 1, false);
assertDuration(d2.with(like7), -9, -8, -7, -6, -5, -4, -3, -2, -1, -10, -1,
    false);
// Different sign
assertThrows(() => d2.with({years: -1}), RangeError);
assertThrows(() => d2.with({months: -2}), RangeError);
assertThrows(() => d2.with({weeks: -3}), RangeError);
assertThrows(() => d2.with({days: -4}), RangeError);
assertThrows(() => d2.with({hours: -5}), RangeError);
assertThrows(() => d2.with({minutes: -6}), RangeError);
assertThrows(() => d2.with({seconds: -7}), RangeError);
assertThrows(() => d2.with({milliseconds: -8}), RangeError);
assertThrows(() => d2.with({microseconds: -9}), RangeError);
assertThrows(() => d2.with({nanoseconds: -10}), RangeError);


// Test large number
let d3 = new Temporal.Duration(1e5, 2e5, 3e5, 4e5, 5e5, 6e5, 7e5, 8e5, 9e5,
    10e5);
assertDuration(d3, 1e5, 2e5, 3e5, 4e5, 5e5, 6e5, 7e5, 8e5, 9e5, 10e5, 1,
    false);
assertDuration(d3.with(like1), 9, 8, 7, 6, 5, 4, 3, 2, 1, 10, 1, false);
assertDuration(d3.with(like7), -9, -8, -7, -6, -5, -4, -3, -2, -1, -10, -1,
    false);

// Test negative values
let d4 = new Temporal.Duration(-1, -2, -3, -4, -5, -6, -7, -8, -9, -10);
assertDuration(d4, -1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -1, false);
assertDuration(d4.with(like1), 9, 8, 7, 6, 5, 4, 3, 2, 1, 10, 1, false);
// Throw when sign flip
assertThrows(() => d4.with({years: 1}), RangeError);
assertThrows(() => d4.with({months: 2}), RangeError);
assertThrows(() => d4.with({weeks: 3}), RangeError);
assertThrows(() => d4.with({days: 4}), RangeError);
assertThrows(() => d4.with({hours: 5}), RangeError);
assertThrows(() => d4.with({minutes: 6}), RangeError);
assertThrows(() => d4.with({seconds: 7}), RangeError);
assertThrows(() => d4.with({milliseconds: 8}), RangeError);
assertThrows(() => d4.with({microseconds: 9}), RangeError);
assertThrows(() => d4.with({nanoseconds: 10}), RangeError);

// singular throw
assertThrows(() => d1.with({year:1}), TypeError);
assertThrows(() => d1.with({month:1}), TypeError);
assertThrows(() => d1.with({week:1}), TypeError);
assertThrows(() => d1.with({day:1}), TypeError);
assertThrows(() => d1.with({hour:1}), TypeError);
assertThrows(() => d1.with({minute:1}), TypeError);
assertThrows(() => d1.with({second:1}), TypeError);
assertThrows(() => d1.with({millisecond:1}), TypeError);
assertThrows(() => d1.with({microsecond:1}), TypeError);
assertThrows(() => d1.with({nanosecond:1}), TypeError);
