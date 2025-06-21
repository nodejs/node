// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

let d1 = new Temporal.PlainDateTime(1911, 10, 10, 4, 5, 6, 7, 8, 9);
let badDate = { withPlainTime: d1.withPlainTime }
assertThrows(() => badDate.withPlainTime(), TypeError);

let timeRecord = {
  hour: 9, minute: 8, second: 7,
  millisecond: 6, microsecond: 5, nanosecond: 4
};

assertPlainDateTime(d1.withPlainTime(timeRecord), 1911, 10, 10, 9, 8, 7, 6, 5, 4);

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

assertPlainDateTime(d3.withPlainTime(timeRecord), 109, 3, 15, 9, 8, 7, 6, 5, 4);

assertThrows(() => d1.withPlainTime(null), RangeError);
assertThrows(() => d1.withPlainTime(true), RangeError);
assertThrows(() => d1.withPlainTime(false), RangeError);
assertThrows(() => d1.withPlainTime(Infinity), RangeError);
assertThrows(() => d1.withPlainTime("invalid iso8601 string"), RangeError);

assertThrows(() => d1.withPlainTime(123), RangeError);
assertThrows(() => d1.withPlainTime(456n), RangeError);
