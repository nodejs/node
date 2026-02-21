// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal


// https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.year
let cal = new Temporal.Calendar("iso8601");

assertEquals(2021, cal.year(new Temporal.PlainDate(2021, 7, 15)));
assertEquals(1997, cal.year(new Temporal.PlainDateTime(
    1997, 8, 23, 5, 30, 13)));
assertEquals(1999, cal.year(new Temporal.PlainYearMonth(1999, 6)));
assertEquals(2019, cal.year("2019-03-15"));

// TODO Test the following later.
//assertEquals(2020, cal.year(new Temporal.ZonedDateTime(86400n * 366n * 50n, "UTC")))
//assertEquals(2001, cal.year({year: 2001, month: 9, day: 11}));
