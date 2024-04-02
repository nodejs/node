// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.daysinyear
let cal = new Temporal.Calendar("iso8601");

assertEquals(365, cal.daysInYear(new Temporal.PlainDate(1995, 7, 15)));
assertEquals(366, cal.daysInYear(new Temporal.PlainDate(1996, 7, 15)));
assertEquals(365, cal.daysInYear(new Temporal.PlainDate(1997, 7, 15)));
assertEquals(365, cal.daysInYear(new Temporal.PlainDate(1998, 7, 15)));
assertEquals(365, cal.daysInYear(new Temporal.PlainDate(1999, 7, 15)));
assertEquals(366, cal.daysInYear(new Temporal.PlainDate(2000, 7, 15)));
assertEquals(365, cal.daysInYear(new Temporal.PlainDate(2001, 7, 15)));
assertEquals(365, cal.daysInYear(new Temporal.PlainDate(2002, 7, 15)));
assertEquals(365, cal.daysInYear(new Temporal.PlainDate(2003, 7, 15)));
assertEquals(366, cal.daysInYear(new Temporal.PlainDate(2004, 7, 15)));
assertEquals(365, cal.daysInYear(new Temporal.PlainDate(2005, 7, 15)));

assertEquals(365,
    cal.daysInYear(new Temporal.PlainDateTime(1995, 8, 23, 5, 30, 13)));
assertEquals(366,
    cal.daysInYear(new Temporal.PlainDateTime(1996, 8, 23, 5, 30, 13)));
assertEquals(365,
    cal.daysInYear(new Temporal.PlainDateTime(1997, 8, 23, 5, 30, 13)));
assertEquals(365,
    cal.daysInYear(new Temporal.PlainDateTime(1998, 8, 23, 5, 30, 13)));
assertEquals(365,
    cal.daysInYear(new Temporal.PlainDateTime(1999, 8, 23, 5, 30, 13)));
assertEquals(366,
    cal.daysInYear(new Temporal.PlainDateTime(2000, 8, 23, 5, 30, 13)));
assertEquals(365,
    cal.daysInYear(new Temporal.PlainDateTime(2001, 8, 23, 5, 30, 13)));
assertEquals(365,
    cal.daysInYear(new Temporal.PlainDateTime(2002, 8, 23, 5, 30, 13)));
assertEquals(365,
    cal.daysInYear(new Temporal.PlainDateTime(2003, 8, 23, 5, 30, 13)));
assertEquals(366,
    cal.daysInYear(new Temporal.PlainDateTime(2004, 8, 23, 5, 30, 13)));
assertEquals(365,
    cal.daysInYear(new Temporal.PlainDateTime(2005, 8, 23, 5, 30, 13)));

assertEquals(365, cal.daysInYear("2019-03-18"));
assertEquals(366, cal.daysInYear("2020-03-18"));
assertEquals(365, cal.daysInYear("2021-03-18"));
assertEquals(365, cal.daysInYear("2022-03-18"));
assertEquals(365, cal.daysInYear("2023-03-18"));
assertEquals(366, cal.daysInYear("2024-03-18"));
assertEquals(365, cal.daysInYear("2025-03-18"));
assertEquals(365, cal.daysInYear("2026-03-18"));

// TODO test the following later
//assertEquals(365, cal.daysInYear(new Temporal.PlainMonthDay(2, 6)));
//assertEquals(365, cal.daysInYear(new Temporal.ZonedDateTime(
//  86400n * 366n * 50n, "UTC")))
//assertEquals(365, cal.daysInYear({year: 2001, month: 9, day: 11}));
