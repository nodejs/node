// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.day
let cal = new Temporal.Calendar("iso8601");

assertEquals(15, cal.day(new Temporal.PlainDate(2021, 7, 15)));
assertEquals(23, cal.day(new Temporal.PlainDateTime(1997, 8, 23, 5, 30, 13)));
assertEquals(6, cal.day(new Temporal.PlainMonthDay(2, 6)));
assertEquals(18, cal.day("2019-03-18"));

// TODO test the following later
//assertEquals(??, cal.day(new Temporal.ZonedDateTime(86400n * 366n * 50n,
//  "UTC")))
//assertEquals(11, cal.day({year: 2001, month: 9, day: 11}));
