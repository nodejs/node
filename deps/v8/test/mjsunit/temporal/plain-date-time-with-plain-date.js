// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

let d1 = new Temporal.PlainDateTime(1911, 10, 10, 4, 5, 6, 7, 8, 9);
let badDate = { withPlainDate: d1.withPlainDate }
assertThrows(() => badDate.withPlainDate("2021-03-11"), TypeError);

assertPlainDateTime(d1.withPlainDate("2021-07-12"),
    2021, 7, 12, 4, 5, 6, 7, 8, 9);

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

let d3 = new Temporal.PlainDateTime(2020, 3, 15, 4, 5, 6, 7, 8, 9, rocCal);

assertPlainDateTime(d3.withPlainDate("2021-07-12"), 110, 7, 12, 4, 5, 6, 7, 8, 9);

assertThrows(() => d1.withPlainDate(undefined), RangeError);
assertThrows(() => d1.withPlainDate(null), RangeError);
assertThrows(() => d1.withPlainDate(true), RangeError);
assertThrows(() => d1.withPlainDate(false), RangeError);
assertThrows(() => d1.withPlainDate(Infinity), RangeError);
assertThrows(() => d1.withPlainDate(123), RangeError);
assertThrows(() => d1.withPlainDate(456n), RangeError);
assertThrows(() => d1.withPlainDate("invalid iso8601 string"), RangeError);
