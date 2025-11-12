// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

let d1 = new Temporal.PlainDate(1911, 10, 10);

assertPlainDate(d1.with({year:2021}), 2021, 10, 10);
assertPlainDate(d1.with({month:11}), 1911, 11, 10);
assertPlainDate(d1.with({monthCode:"M05"}), 1911, 5, 10);
assertPlainDate(d1.with({day:30}), 1911, 10, 30);
assertPlainDate(d1.with({year:2021, hour: 30}), 2021, 10, 10);
assertPlainDate(d1.with({month:11, minute: 71}), 1911, 11, 10);
assertPlainDate(d1.with({monthCode:"M05", second: 90}), 1911, 5, 10);
assertPlainDate(d1.with({day:30, era:"BC" }), 1911, 10, 30);

// A simplified version of Republic of China calendar
let rocCal = {
  iso8601: new Temporal.Calendar("iso8601"),
  get id() {return "roc";},
  dateFromFields: function(fields, options) {
    fields.year -= 1911;
    return this.iso8601.dateFromFields(fields, options);
  },
  year: function(date) { return this.iso8601.year(date) - 1911; },
  month: function(date) { return this.iso8601.month(date); },
  monthCode: function(date) { return this.iso8601.monthCode(date); },
  day: function(date) { return this.iso8601.day(date); },
};

let d2 = new Temporal.PlainDate(2021, 7, 20, rocCal);

assertPlainDate(d2, 110, 7, 20);
assertPlainDate(d2.with({year: 1912}), 1, 7, 20);
assertPlainDate(d2.with({year: 1987}), 76, 7, 20);

assertThrows(() => d1.with(new Temporal.PlainDate(2021, 7, 1)), TypeError);
assertThrows(() => d1.with(new Temporal.PlainDateTime(2021, 7, 1, 12, 13)), TypeError);
assertThrows(() => d1.with(new Temporal.PlainTime(1, 12, 13)), TypeError);
assertThrows(() => d1.with(new Temporal.PlainYearMonth(1991, 12)), TypeError);
assertThrows(() => d1.with(new Temporal.PlainMonthDay(5, 12)), TypeError);
assertThrows(() => d1.with("2012-05-13"), TypeError);
assertThrows(() => d1.with({calendar: "iso8601"}), TypeError);
assertThrows(() => d1.with({timeZone: "UTC"}), TypeError);
assertThrows(() => d1.with(true), TypeError);
assertThrows(() => d1.with(false), TypeError);
assertThrows(() => d1.with(NaN), TypeError);
assertThrows(() => d1.with(Infinity), TypeError);
assertThrows(() => d1.with(1234), TypeError);
assertThrows(() => d1.with(567n), TypeError);
assertThrows(() => d1.with(Symbol()), TypeError);
assertThrows(() => d1.with("string"), TypeError);
assertThrows(() => d1.with({}), TypeError);
assertThrows(() => d1.with([]), TypeError);

let badDate = { with: d1.with }
assertThrows(() => badDate.with({day: 3}), TypeError);
