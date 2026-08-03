// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.dayofyear
let cal = new Temporal.Calendar("iso8601");

assertEquals(1, cal.dayOfYear(new Temporal.PlainDate(1970, 1, 1)));
assertEquals(1, cal.dayOfYear(new Temporal.PlainDate(2000, 1, 1)));

assertEquals(15, cal.dayOfYear(new Temporal.PlainDate(2021, 1, 15)));
assertEquals(46, cal.dayOfYear(new Temporal.PlainDate(2020, 2, 15)));
assertEquals(46, cal.dayOfYear(new Temporal.PlainDate(2000, 2, 15)));
assertEquals(75, cal.dayOfYear(new Temporal.PlainDate(2020, 3, 15)));
assertEquals(75, cal.dayOfYear(new Temporal.PlainDate(2000, 3, 15)));
assertEquals(74, cal.dayOfYear(new Temporal.PlainDate(2001, 3, 15)));
assertEquals(366, cal.dayOfYear(new Temporal.PlainDate(2000, 12, 31)));
assertEquals(365, cal.dayOfYear(new Temporal.PlainDate(2001, 12, 31)));

assertEquals(23,
    cal.dayOfYear(new Temporal.PlainDateTime(1997, 1, 23, 5, 30, 13)));
assertEquals(54,
    cal.dayOfYear(new Temporal.PlainDateTime(1997, 2, 23, 5, 30, 13)));
assertEquals(83,
    cal.dayOfYear(new Temporal.PlainDateTime(1996, 3, 23, 5, 30, 13)));
assertEquals(82,
    cal.dayOfYear(new Temporal.PlainDateTime(1997, 3, 23, 5, 30, 13)));
assertEquals(365,
    cal.dayOfYear(new Temporal.PlainDateTime(1997, 12, 31, 5, 30, 13)));
assertEquals(366,
    cal.dayOfYear(new Temporal.PlainDateTime(1996, 12, 31, 5, 30, 13)));

assertEquals(18, cal.dayOfYear("2019-01-18"));
assertEquals(49, cal.dayOfYear("2020-02-18"));
assertEquals(365, cal.dayOfYear("2019-12-31"));
assertEquals(366, cal.dayOfYear("2000-12-31"));

// TODO test the following later
//assertEquals(7, cal.dayOfYear(new Temporal.PlainMonthDay(2, 6)));
//assertEquals(31, cal.dayOfYear(new Temporal.ZonedDateTime(
//  86400n * 366n * 50n, "UTC")))
//assertEquals(30, cal.dayOfYear({year: 2001, month: 9, day: 11}));
