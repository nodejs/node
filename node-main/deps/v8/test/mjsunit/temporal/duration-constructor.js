// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

let d1 = new Temporal.Duration();
assertDuration(d1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, true);

let d2 = new Temporal.Duration(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
assertDuration(d2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1, false);

// Test large number
let d3 = new Temporal.Duration(
    1e5, 2e5, 3e5, 4e5, 5e5, 6e5, 7e5, 8e5, 9e5, 10e5);
assertDuration(
    d3, 1e5, 2e5, 3e5, 4e5, 5e5, 6e5, 7e5, 8e5, 9e5, 10e5, 1, false);

// Test negative values
let d4 = new Temporal.Duration(
    -1, -2, -3, -4, -5, -6, -7, -8, -9, -10);
assertDuration(
    d4, -1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -1, false);

// Test NaN
let d5 = new Temporal.Duration(NaN, NaN, NaN);
assertDuration(d5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, true);
// 1. If NewTarget is undefined, then
// a. Throw a TypeError exception.
assertThrows(() => Temporal.Duration(), TypeError);

// 1. Let number be ? ToNumber(argument).
assertDuration(new Temporal.Duration(undefined, 234, true, false, "567"),
    0, 234, 1, 0, 567, 0, 0, 0, 0, 0, 1, false);
assertThrows(() => new Temporal.Duration(Symbol(123)), TypeError);
assertThrows(() => new Temporal.Duration(123n), TypeError);

// Test Infinity
// 7.5.4 IsValidDuration ( years, months, weeks, days, hours, minutes, seconds,
// milliseconds, microseconds, nanoseconds )
// a. If v is not finite, return false.
assertThrows(() => new Temporal.Duration(1, 2, 3, 4, 5, 6, 7, 8, 9, Infinity),
    RangeError);
assertThrows(() => new Temporal.Duration(1, 2, 3, 4, 5, 6, 7, 8, Infinity),
    RangeError);
assertThrows(() => new Temporal.Duration(1, 2, 3, 4, 5, 6, 7, Infinity),
    RangeError);
assertThrows(() => new Temporal.Duration(1, 2, 3, 4, 5, 6, Infinity),
    RangeError);
assertThrows(() => new Temporal.Duration(1, 2, 3, 4, 5, Infinity), RangeError);
assertThrows(() => new Temporal.Duration(1, 2, 3, 4, Infinity), RangeError);
assertThrows(() => new Temporal.Duration(1, 2, 3, Infinity), RangeError);
assertThrows(() => new Temporal.Duration(1, 2, Infinity), RangeError);
assertThrows(() => new Temporal.Duration(1, Infinity), RangeError);
assertThrows(() => new Temporal.Duration(Infinity), RangeError);
assertThrows(() => new Temporal.Duration(-1, -2, -3, -4, -5, -6, -7, -8, -9,
      -Infinity), RangeError);
assertThrows(() => new Temporal.Duration(-1, -2, -3, -4, -5, -6, -7, -8,
      -Infinity), RangeError);
assertThrows(() => new Temporal.Duration(-1, -2, -3, -4, -5, -6, -7,
      -Infinity), RangeError);
assertThrows(() => new Temporal.Duration(-1, -2, -3, -4, -5, -6, -Infinity),
    RangeError);
assertThrows(() => new Temporal.Duration(-1, -2, -3, -4, -5, -Infinity),
    RangeError);
assertThrows(() => new Temporal.Duration(-1, -2, -3, -4, -Infinity),
    RangeError);
assertThrows(() => new Temporal.Duration(-1, -2, -3, -Infinity), RangeError);
assertThrows(() => new Temporal.Duration(-1, -2, -Infinity), RangeError);
assertThrows(() => new Temporal.Duration(-1, -Infinity), RangeError);
assertThrows(() => new Temporal.Duration(-Infinity), RangeError);

// Sign different
assertThrows(() => new Temporal.Duration(1, -2), RangeError);
assertThrows(() => new Temporal.Duration(1, 0, -2), RangeError);
assertThrows(() => new Temporal.Duration(-1, 0, 0, 3), RangeError);
assertThrows(() => new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 1, -1),
    RangeError);
assertThrows(() => new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, -1, 1),
    RangeError);
