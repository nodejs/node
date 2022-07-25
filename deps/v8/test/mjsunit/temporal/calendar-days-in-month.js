// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.daysinmonth
let cal = new Temporal.Calendar("iso8601");

assertEquals(31, cal.daysInMonth(new Temporal.PlainDate(2021, 1, 15)));
// leap year
assertEquals(29, cal.daysInMonth(new Temporal.PlainDate(2020, 2, 15)));
assertEquals(29, cal.daysInMonth(new Temporal.PlainDate(2000, 2, 15)));
// non-leap year
assertEquals(28, cal.daysInMonth(new Temporal.PlainDate(2021, 2, 15)));
assertEquals(31, cal.daysInMonth(new Temporal.PlainDate(2021, 3, 15)));
assertEquals(30, cal.daysInMonth(new Temporal.PlainDate(2021, 4, 15)));
assertEquals(31, cal.daysInMonth(new Temporal.PlainDate(2021, 5, 15)));
assertEquals(30, cal.daysInMonth(new Temporal.PlainDate(2021, 6, 15)));
assertEquals(31, cal.daysInMonth(new Temporal.PlainDate(2021, 7, 15)));
assertEquals(31, cal.daysInMonth(new Temporal.PlainDate(2021, 8, 15)));
assertEquals(30, cal.daysInMonth(new Temporal.PlainDate(2021, 9, 15)));
assertEquals(31, cal.daysInMonth(new Temporal.PlainDate(2021, 10, 15)));
assertEquals(30, cal.daysInMonth(new Temporal.PlainDate(2021, 11, 15)));
assertEquals(31, cal.daysInMonth(new Temporal.PlainDate(2021, 12, 15)));

assertEquals(31,
    cal.daysInMonth(new Temporal.PlainDateTime(1997, 1, 23, 5, 30, 13)));
// leap year
assertEquals(29,
    cal.daysInMonth(new Temporal.PlainDateTime(1996, 2, 23, 5, 30, 13)));
assertEquals(29,
    cal.daysInMonth(new Temporal.PlainDateTime(2000, 2, 23, 5, 30, 13)));
// non leap year
assertEquals(28,
    cal.daysInMonth(new Temporal.PlainDateTime(1997, 2, 23, 5, 30, 13)));
assertEquals(31,
    cal.daysInMonth(new Temporal.PlainDateTime(1997, 3, 23, 5, 30, 13)));
assertEquals(30,
    cal.daysInMonth(new Temporal.PlainDateTime(1997, 4, 23, 5, 30, 13)));
assertEquals(31,
    cal.daysInMonth(new Temporal.PlainDateTime(1997, 5, 23, 5, 30, 13)));
assertEquals(30,
    cal.daysInMonth(new Temporal.PlainDateTime(1997, 6, 23, 5, 30, 13)));
assertEquals(31,
    cal.daysInMonth(new Temporal.PlainDateTime(1997, 7, 23, 5, 30, 13)));
assertEquals(31,
    cal.daysInMonth(new Temporal.PlainDateTime(1997, 8, 23, 5, 30, 13)));
assertEquals(30,
    cal.daysInMonth(new Temporal.PlainDateTime(1997, 9, 23, 5, 30, 13)));
assertEquals(31,
    cal.daysInMonth(new Temporal.PlainDateTime(1997, 10, 23, 5, 30, 13)));
assertEquals(30,
    cal.daysInMonth(new Temporal.PlainDateTime(1997, 11, 23, 5, 30, 13)));
assertEquals(31,
    cal.daysInMonth(new Temporal.PlainDateTime(1997, 12, 23, 5, 30, 13)));

assertEquals(31, cal.daysInMonth("2019-01-18"));
// leap year
assertEquals(29, cal.daysInMonth("2020-02-18"));
// non leap
assertEquals(28, cal.daysInMonth("2019-02-18"));
assertEquals(31, cal.daysInMonth("2019-03-18"));
assertEquals(30, cal.daysInMonth("2019-04-18"));
assertEquals(31, cal.daysInMonth("2019-05-18"));
assertEquals(30, cal.daysInMonth("2019-06-18"));
assertEquals(31, cal.daysInMonth("2019-07-18"));
assertEquals(31, cal.daysInMonth("2019-08-18"));
assertEquals(30, cal.daysInMonth("2019-09-18"));
assertEquals(31, cal.daysInMonth("2019-10-18"));
assertEquals(30, cal.daysInMonth("2019-11-18"));
assertEquals(31, cal.daysInMonth("2019-12-18"));

// TODO test the following later
//assertEquals(7, cal.daysInMonth(new Temporal.PlainMonthDay(2, 6)));
//assertEquals(31, cal.daysInMonth(new Temporal.ZonedDateTime(
//  86400n * 366n * 50n, "UTC")))
//assertEquals(30, cal.daysInMonth({year: 2001, month: 9, day: 11}));
