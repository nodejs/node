// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.month
let cal = new Temporal.Calendar("iso8601");

assertEquals(7, cal.month(new Temporal.PlainDate(2021, 7, 15)));
assertEquals(8, cal.month(new Temporal.PlainDateTime(1997, 8, 23, 5, 30, 13)));
assertEquals(6, cal.month(new Temporal.PlainYearMonth(1999, 6)));
assertEquals(3, cal.month("2019-03-15"));
assertThrows(() => cal.month(new Temporal.PlainMonthDay(3, 16)), TypeError);

// TODO Test the following later.
//assertEquals(1, cal.month(new Temporal.ZonedDateTime(86400n * 366n * 50n,
//  "UTC")))
//assertEquals(9, cal.month({year: 2001, month: 9, day: 11}));
