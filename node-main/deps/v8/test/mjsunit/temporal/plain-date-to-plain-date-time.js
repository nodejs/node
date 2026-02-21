// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

let d1 = new Temporal.PlainDate(2021, 12, 11);
let badDate = { toPlainDateTime: d1.toPlainDateTime }
assertThrows(() => badDate.toPlainDateTime(), TypeError);

assertThrows(() => d1.toPlainDateTime(null), RangeError);
assertThrows(() => d1.toPlainDateTime("string is invalid"), RangeError);
assertThrows(() => d1.toPlainDateTime(true), RangeError);
assertThrows(() => d1.toPlainDateTime(false), RangeError);
assertThrows(() => d1.toPlainDateTime(NaN), RangeError);
assertThrows(() => d1.toPlainDateTime(Infinity), RangeError);
assertThrows(() => d1.toPlainDateTime(123), RangeError);
assertThrows(() => d1.toPlainDateTime(456n), RangeError);
assertThrows(() => d1.toPlainDateTime(Symbol()), TypeError);
assertThrows(() => d1.toPlainDateTime({}), TypeError);
assertPlainDateTime(d1.toPlainDateTime(
    {hour: 23}),
    2021, 12, 11, 23, 0, 0, 0, 0, 0);
assertPlainDateTime(d1.toPlainDateTime(
    {minute: 23}),
    2021, 12, 11, 0, 23, 0, 0, 0, 0);
assertPlainDateTime(d1.toPlainDateTime(
    {second: 23}),
    2021, 12, 11, 0, 0, 23, 0, 0, 0);
assertPlainDateTime(d1.toPlainDateTime(
    {millisecond: 23}),
    2021, 12, 11, 0, 0, 0, 23, 0, 0);
assertPlainDateTime(d1.toPlainDateTime(
    {microsecond: 23}),
    2021, 12, 11, 0, 0, 0, 0, 23, 0);
assertPlainDateTime(d1.toPlainDateTime(
    {nanosecond: 23}),
    2021, 12, 11, 0, 0, 0, 0, 0, 23);
assertPlainDateTime(d1.toPlainDateTime(),
    2021, 12, 11, 0, 0, 0, 0, 0, 0);
assertPlainDateTime(d1.toPlainDateTime(
    {hour: 9, minute: 8, second: 7, millisecond: 6, microsecond: 5, nanosecond: 4}),
    2021, 12, 11, 9, 8, 7, 6, 5, 4);
