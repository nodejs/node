// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

let d1 = new Temporal.PlainDateTime(1911, 10, 10);
assertPlainDateTime(d1, 1911, 10, 10, 0, 0, 0, 0, 0, 0);
let d2 = new Temporal.PlainDateTime(2020, 3, 12);
assertPlainDateTime(d2, 2020, 3, 12, 0, 0, 0, 0, 0, 0);
let d3 = new Temporal.PlainDateTime(1, 12, 25);
assertPlainDateTime(d3, 1, 12, 25, 0, 0, 0, 0, 0, 0);
let d4 = new Temporal.PlainDateTime(1970, 1, 1);
assertPlainDateTime(d4, 1970, 1, 1, 0, 0, 0, 0, 0, 0);
let d5 = new Temporal.PlainDateTime(-10, 12, 1);
assertPlainDateTime(d5, -10, 12, 1, 0, 0, 0, 0, 0, 0);
let d6 = new Temporal.PlainDateTime(-25406, 1, 1);
assertPlainDateTime(d6, -25406, 1, 1, 0, 0, 0, 0, 0, 0);
let d7 = new Temporal.PlainDateTime(26890, 12, 31);
assertPlainDateTime(d7, 26890, 12, 31, 0, 0, 0, 0, 0, 0);

assertThrows(() => Temporal.PlainDateTime(2021, 7, 1), TypeError);
assertThrows(() => new Temporal.PlainDateTime(), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 0), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 13), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7, 0), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7, 32), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, -7, 1), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, -7, -1), RangeError);
// Wrong month
assertThrows(() => new Temporal.PlainDateTime(2021, 0, 1), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 13, 1), RangeError);
// Right day for month
assertPlainDateTime((new Temporal.PlainDateTime(2021, 1, 31)),
    2021, 1, 31, 0, 0, 0, 0, 0, 0);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 2, 28)),
    2021, 2, 28, 0, 0, 0, 0, 0, 0);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 3, 31)),
    2021, 3, 31, 0, 0, 0, 0, 0, 0);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 4, 30)),
    2021, 4, 30, 0, 0, 0, 0, 0, 0);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 5, 31)),
    2021, 5, 31, 0, 0, 0, 0, 0, 0);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 6, 30)),
    2021, 6, 30, 0, 0, 0, 0, 0, 0);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 31)),
    2021, 7, 31, 0, 0, 0, 0, 0, 0);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 8, 31)),
    2021, 8, 31, 0, 0, 0, 0, 0, 0);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 9, 30)),
    2021, 9, 30, 0, 0, 0, 0, 0, 0);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 10, 31)),
    2021, 10, 31, 0, 0, 0, 0, 0, 0);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 11, 30)),
    2021, 11, 30, 0, 0, 0, 0, 0, 0);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 12, 31)),
    2021, 12, 31, 0, 0, 0, 0, 0, 0);

// Check Feb 29 for Leap year
assertThrows(() => new Temporal.PlainDateTime(1900, 2, 29), RangeError);
assertPlainDateTime((new Temporal.PlainDateTime(2000, 2, 29)),
    2000, 2, 29, 0, 0, 0, 0, 0, 0);
assertThrows(() => new Temporal.PlainDateTime(2001, 2, 29), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2002, 2, 29), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2003, 2, 29), RangeError);
assertPlainDateTime((new Temporal.PlainDateTime(2004, 2, 29)),
    2004, 2, 29, 0, 0, 0, 0, 0, 0);
assertThrows(() => new Temporal.PlainDateTime(2100, 2, 29), RangeError);

// Wrong day for month
assertThrows(() => new Temporal.PlainDateTime(2021, 1, 32), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 2, 29), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 3, 32), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 4, 31), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 5, 32), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 6, 31), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7, 32), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 8, 32), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 9, 31), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 10, 32), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 11, 31), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 12, 32), RangeError);

// Infinity
assertThrows(() => new Temporal.PlainDateTime(Infinity, 12, 1), RangeError);
assertThrows(() => new Temporal.PlainDateTime(-Infinity, 12, 1), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 12, Infinity), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 12, -Infinity), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, -Infinity, 1), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, Infinity, 1), RangeError);

assertPlainDateTime(new Temporal.PlainDateTime(2021, 7, 9),
    2021, 7, 9, 0, 0, 0, 0, 0, 0);

assertPlainDateTime(new Temporal.PlainDateTime(2021, 7, 9, 1, 2, 3, 4, 5, 6),
    2021, 7, 9, 1, 2, 3, 4, 5, 6);

assertPlainDateTime(
    new Temporal.PlainDateTime(2021, 7, 9, 1, 2, 3, 4, 5),
    2021, 7, 9, 1, 2, 3, 4, 5, 0);

assertPlainDateTime(
    new Temporal.PlainDateTime(2021, 7, 9, 1, 2, 3, 4),
    2021, 7, 9, 1, 2, 3, 4, 0, 0);

assertPlainDateTime(
    new Temporal.PlainDateTime(2021, 7, 9, 1, 2, 3),
    2021, 7, 9, 1, 2, 3, 0, 0, 0);

assertPlainDateTime(
    new Temporal.PlainDateTime(2021, 7, 9, 1, 2),
    2021, 7, 9, 1, 2, 0, 0, 0, 0);

assertPlainDateTime(
    new Temporal.PlainDateTime(2021, 7, 9, 1, 0),
    2021, 7, 9, 1, 0, 0, 0, 0, 0);

// smallest values
assertPlainDateTime(
    new Temporal.PlainDateTime(2021, 7, 9, 0, 0, 0, 0, 0, 0),
    2021, 7, 9, 0, 0, 0, 0, 0, 0);

// largest values
assertPlainDateTime(
    new Temporal.PlainDateTime(2021, 7, 9, 23, 59, 59, 999, 999, 999),
    2021, 7, 9, 23, 59, 59, 999, 999, 999);

assertPlainDateTime(
    new Temporal.PlainDateTime(2021, 7, 9, true, false, NaN, undefined, true),
    2021, 7, 9, 1, 0, 0, 0, 1, 0);

assertPlainDateTime(
    new Temporal.PlainDateTime(2021, 7, 9, 11.9, 12.8, 13.7, 14.6, 15.5, 1.999999),
    2021, 7, 9, 11, 12, 13, 14, 15, 1);

assertThrows(() => new Temporal.PlainDateTime(2021, 7, 9, -Infinity), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7, 9, Infinity), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7, 9, Symbol(2)), TypeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7, 9, 3n), TypeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7, 9, 24), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7, 9, 0, 60), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7, 9, 0, 0, 60), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7, 9, 0, 0, 0, 1000), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7, 9, 0, 0, 0, 0, 1000), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7, 9, 0, 0, 0, 0, 0, 1000), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7, 9, -1), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7, 9, 0, -1), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7, 9, 0, 0, -1), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7, 9, 0, 0, 0, -1), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7, 9, 0, 0, 0, 0, -1), RangeError);
assertThrows(() => new Temporal.PlainDateTime(2021, 7, 9, 0, 0, 0, 0, 0, -1), RangeError);
