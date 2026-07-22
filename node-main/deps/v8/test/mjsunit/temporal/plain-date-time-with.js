// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

let d1 = new Temporal.PlainDateTime(1911, 11, 10, 4, 5, 6, 7, 8, 9);
let badDate = { with: d1.with }
assertThrows(() => badDate.with(), TypeError);

assertThrows(() => d1.with(null), TypeError);
assertThrows(() => d1.with(undefined), TypeError);
assertThrows(() => d1.with("string is invalid"), TypeError);
assertThrows(() => d1.with(true), TypeError);
assertThrows(() => d1.with(false), TypeError);
assertThrows(() => d1.with(NaN), TypeError);
assertThrows(() => d1.with(Infinity), TypeError);
assertThrows(() => d1.with(123), TypeError);
assertThrows(() => d1.with(456n), TypeError);
assertThrows(() => d1.with(Symbol()), TypeError);
let date = Temporal.Now.plainDateISO();
assertThrows(() => d1.with(date), TypeError);
let dateTime = Temporal.Now.plainDateTimeISO();
assertThrows(() => d1.with(dateTime), TypeError);
let time = Temporal.Now.plainTimeISO();
assertThrows(() => d1.with(time), TypeError);
let ym = new Temporal.PlainYearMonth(2021, 7);
assertThrows(() => d1.with(ym), TypeError);
let md = new Temporal.PlainMonthDay(12, 25);
assertThrows(() => d1.with(md), TypeError);
assertThrows(() => d1.with({calendar: "iso8601"}), TypeError);
assertThrows(() => d1.with({timeZone: "UTC"}), TypeError);
// options is not undefined or object
assertThrows(() => d1.with({day: 3}, null), TypeError);
assertThrows(() => d1.with({day: 3}, "string is invalid"), TypeError);
assertThrows(() => d1.with({day: 3}, true), TypeError);
assertThrows(() => d1.with({day: 3}, false), TypeError);
assertThrows(() => d1.with({day: 3}, 123), TypeError);
assertThrows(() => d1.with({day: 3}, 456n), TypeError);
assertThrows(() => d1.with({day: 3}, Symbol()), TypeError);
assertThrows(() => d1.with({day: 3}, NaN), TypeError);
assertThrows(() => d1.with({day: 3}, Infinity), TypeError);

assertPlainDateTime(d1.with({year: 2021}), 2021, 11, 10, 4, 5, 6, 7, 8, 9);
assertPlainDateTime(d1.with({month: 3}), 1911, 3, 10, 4, 5, 6, 7, 8, 9);
assertPlainDateTime(d1.with({monthCode: "M05"}), 1911, 5, 10, 4, 5, 6, 7, 8, 9);
assertPlainDateTime(d1.with({day: 1}), 1911, 11, 1, 4, 5, 6, 7, 8, 9);
assertPlainDateTime(d1.with({hour: 2}), 1911, 11, 10, 2, 5, 6, 7, 8, 9);
assertPlainDateTime(d1.with({minute: 3}), 1911, 11, 10, 4, 3, 6, 7, 8, 9);
assertPlainDateTime(d1.with({second: 4}), 1911, 11, 10, 4, 5, 4, 7, 8, 9);
assertPlainDateTime(d1.with({millisecond: 5}), 1911, 11, 10, 4, 5, 6, 5, 8, 9);
assertPlainDateTime(d1.with({microsecond: 6}), 1911, 11, 10, 4, 5, 6, 7, 6, 9);
assertPlainDateTime(d1.with({nanosecond: 7}), 1911, 11, 10, 4, 5, 6, 7, 8, 7);
