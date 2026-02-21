// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.monthsinyear
let cal = new Temporal.Calendar("iso8601");

assertEquals(12, cal.monthsInYear(new Temporal.PlainDate(2021, 7, 15)));
assertEquals(12, cal.monthsInYear(new Temporal.PlainDate(1234, 7, 15)));
assertEquals(12,
    cal.monthsInYear(new Temporal.PlainDateTime(1997, 8, 23, 5, 30, 13)));
assertEquals(12,
    cal.monthsInYear(new Temporal.PlainDateTime(1234, 8, 23, 5, 30, 13)));
assertEquals(12, cal.monthsInYear("2019-03-18"));
assertEquals(12, cal.monthsInYear("1234-03-18"));

// TODO Test the following later.
//assertEquals(12, cal.monthsInYear(new Temporal.PlainMonthDay(2, 6)));
//assertEquals(12, cal.monthsInYear(new Temporal.ZonedDateTime(
//  86400n * 366n * 50n, "UTC")))
//assertEquals(12, cal.monthsInYear({year: 2001, month: 9, day: 11}));
