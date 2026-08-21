// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.weekofyear

let cal = new Temporal.Calendar("iso8601");

// From https://en.wikipedia.org/wiki/ISO_week_date#Relation_with_the_Gregorian_calendar
assertEquals(53, cal.weekOfYear(new Temporal.PlainDate(1977, 01, 01)));
assertEquals(53, cal.weekOfYear(new Temporal.PlainDate(1977, 01, 02)));

assertEquals(52, cal.weekOfYear(new Temporal.PlainDate(1977, 12, 31)));
assertEquals(52, cal.weekOfYear(new Temporal.PlainDate(1978, 01, 01)));
assertEquals(1, cal.weekOfYear(new Temporal.PlainDate(1978, 01, 02)));

assertEquals(52, cal.weekOfYear(new Temporal.PlainDate(1978, 12, 31)));
assertEquals(1, cal.weekOfYear(new Temporal.PlainDate(1979, 01, 01)));

assertEquals(52, cal.weekOfYear(new Temporal.PlainDate(1979, 12, 30)));
assertEquals(1, cal.weekOfYear(new Temporal.PlainDate(1979, 12, 31)));
assertEquals(1, cal.weekOfYear(new Temporal.PlainDate(1980, 01, 01)));

assertEquals(52, cal.weekOfYear(new Temporal.PlainDate(1980, 12, 28)));
assertEquals(1, cal.weekOfYear(new Temporal.PlainDate(1980, 12, 29)));
assertEquals(1, cal.weekOfYear(new Temporal.PlainDate(1980, 12, 30)));
assertEquals(1, cal.weekOfYear(new Temporal.PlainDate(1980, 12, 31)));
assertEquals(1, cal.weekOfYear(new Temporal.PlainDate(1981, 01, 01)));

assertEquals(53, cal.weekOfYear(new Temporal.PlainDate(1981, 12, 31)));
assertEquals(53, cal.weekOfYear(new Temporal.PlainDate(1982, 01, 01)));
assertEquals(53, cal.weekOfYear(new Temporal.PlainDate(1982, 01, 02)));
assertEquals(53, cal.weekOfYear(new Temporal.PlainDate(1982, 01, 03)));


assertEquals(53, cal.weekOfYear("1977-01-01"));
assertEquals(53, cal.weekOfYear("1977-01-02"));

assertEquals(52, cal.weekOfYear("1977-12-31"));
assertEquals(52, cal.weekOfYear("1978-01-01"));
assertEquals(1, cal.weekOfYear("1978-01-02"));

assertEquals(52, cal.weekOfYear("1978-12-31"));
assertEquals(1, cal.weekOfYear("1979-01-01"));

assertEquals(52, cal.weekOfYear("1979-12-30"));
assertEquals(1, cal.weekOfYear("1979-12-31"));
assertEquals(1, cal.weekOfYear("1980-01-01"));

assertEquals(52, cal.weekOfYear("1980-12-28"));
assertEquals(1, cal.weekOfYear("1980-12-29"));
assertEquals(1, cal.weekOfYear("1980-12-30"));
assertEquals(1, cal.weekOfYear("1980-12-31"));
assertEquals(1, cal.weekOfYear("1981-01-01"));

assertEquals(53, cal.weekOfYear("1981-12-31"));
assertEquals(53, cal.weekOfYear("1982-01-01"));
assertEquals(53, cal.weekOfYear("1982-01-02"));
assertEquals(53, cal.weekOfYear("1982-01-03"));

// TODO test the following later
//assertEquals(4, cal.weekOfYear(new Temporal.PlainDateTime(1997, 1, 23, 5,
//  30, 13)));
//assertEquals(7, cal.weekOfYear(new Temporal.PlainMonthDay(2, 6)));
//assertEquals(31, cal.weekOfYear(new Temporal.ZonedDateTime(
//  86400n * 366n * 50n, "UTC")))
//assertEquals(30, cal.weekOfYear({year: 2001, month: 9, day: 11}));
