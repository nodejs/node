// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

let d1 = new Temporal.PlainDateTime(1911, 10, 10, 4, 5, 6, 7, 8, 9);
let badDateTime = { withCalendar: d1.withCalendar }
assertThrows(() => badDateTime.withCalendar("iso8601"), TypeError);

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

let d2 = d1.withCalendar(rocCal);
assertEquals(d2.calendar.id, "roc");
assertPlainDateTime(d2, 0, 10, 10, 4, 5, 6, 7, 8, 9);

let d3 = d2.withCalendar("iso8601");
assertEquals(d3.calendar.id, "iso8601");
assertPlainDateTime(d3, 1911, 10, 10, 4, 5, 6, 7, 8, 9);
