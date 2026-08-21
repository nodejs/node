// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.monthcode
let cal = new Temporal.Calendar("iso8601");

assertEquals("M07", cal.monthCode(new Temporal.PlainDate(2021, 7, 15)));
assertEquals("M08",
    cal.monthCode(new Temporal.PlainDateTime(1997, 8, 23, 5, 30, 13)));
assertEquals("M06", cal.monthCode(new Temporal.PlainYearMonth(1999, 6)));
assertEquals("M02", cal.monthCode(new Temporal.PlainMonthDay(2, 6)));
assertEquals("M03", cal.monthCode("2019-03-15"));

// TODO Test the following later
//assertEquals("M01", cal.monthCode(new Temporal.ZonedDateTime(
//  86400n * 366n * 50n, "UTC")))
//assertEquals("M09", cal.monthCode({year: 2001, month: 9, day: 11}));
