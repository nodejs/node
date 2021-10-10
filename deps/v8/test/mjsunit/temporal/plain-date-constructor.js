// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

let d1 = new Temporal.PlainDate(1911, 10, 10);
assertPlainDate(d1, 1911, 10, 10);
let d2 = new Temporal.PlainDate(2020, 3, 12);
assertPlainDate(d2, 2020, 3, 12);
let d3 = new Temporal.PlainDate(1, 12, 25);
assertPlainDate(d3, 1, 12, 25);
let d4 = new Temporal.PlainDate(1970, 1, 1);
assertPlainDate(d4, 1970, 1, 1);
let d5 = new Temporal.PlainDate(-10, 12, 1);
assertPlainDate(d5, -10, 12, 1);
let d6 = new Temporal.PlainDate(-25406, 1, 1);
assertPlainDate(d6, -25406, 1, 1);
let d7 = new Temporal.PlainDate(26890, 12, 31);
assertPlainDate(d7, 26890, 12, 31);

assertThrows(() => Temporal.PlainDate(2021, 7, 1), TypeError);
assertThrows(() => new Temporal.PlainDate(), RangeError);
assertThrows(() => new Temporal.PlainDate(2021), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 0), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 7), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 13), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 7, 0), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 7, 32), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, -7, 1), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, -7, -1), RangeError);
// Wrong month
assertThrows(() => new Temporal.PlainDate(2021, 0, 1), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 13, 1), RangeError);
// Right day for month
assertPlainDate((new Temporal.PlainDate(2021, 1, 31)), 2021, 1, 31);
assertPlainDate((new Temporal.PlainDate(2021, 2, 28)), 2021, 2, 28);
assertPlainDate((new Temporal.PlainDate(2021, 3, 31)), 2021, 3, 31);
assertPlainDate((new Temporal.PlainDate(2021, 4, 30)), 2021, 4, 30);
assertPlainDate((new Temporal.PlainDate(2021, 5, 31)), 2021, 5, 31);
assertPlainDate((new Temporal.PlainDate(2021, 6, 30)), 2021, 6, 30);
assertPlainDate((new Temporal.PlainDate(2021, 7, 31)), 2021, 7, 31);
assertPlainDate((new Temporal.PlainDate(2021, 8, 31)), 2021, 8, 31);
assertPlainDate((new Temporal.PlainDate(2021, 9, 30)), 2021, 9, 30);
assertPlainDate((new Temporal.PlainDate(2021, 10, 31)), 2021, 10, 31);
assertPlainDate((new Temporal.PlainDate(2021, 11, 30)), 2021, 11, 30);
assertPlainDate((new Temporal.PlainDate(2021, 12, 31)), 2021, 12, 31);

// Check Feb 29 for Leap year
assertThrows(() => new Temporal.PlainDate(1900, 2, 29), RangeError);
assertPlainDate((new Temporal.PlainDate(2000, 2, 29)), 2000, 2, 29);
assertThrows(() => new Temporal.PlainDate(2001, 2, 29), RangeError);
assertThrows(() => new Temporal.PlainDate(2002, 2, 29), RangeError);
assertThrows(() => new Temporal.PlainDate(2003, 2, 29), RangeError);
assertPlainDate((new Temporal.PlainDate(2004, 2, 29)), 2004, 2, 29);
assertThrows(() => new Temporal.PlainDate(2100, 2, 29), RangeError);

// Wrong day for month
assertThrows(() => new Temporal.PlainDate(2021, 1, 32), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 2, 29), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 3, 32), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 4, 31), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 5, 32), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 6, 31), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 7, 32), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 8, 32), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 9, 31), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 10, 32), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 11, 31), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 12, 32), RangeError);

// Infinty
assertThrows(() => new Temporal.PlainDate(Infinity, 12, 1), RangeError);
assertThrows(() => new Temporal.PlainDate(-Infinity, 12, 1), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 12, Infinity), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, 12, -Infinity), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, -Infinity, 1), RangeError);
assertThrows(() => new Temporal.PlainDate(2021, Infinity, 1), RangeError);

// TODO Test calendar
//
