// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// https://tc39.es/proposal-temporal/#sec-temporal.calendar.from
// 1. If NewTarget is undefined, then
// a. Throw a TypeError exception.

assertEquals("iso8601",
    (Temporal.Calendar.from("iso8601")).id);
assertEquals("iso8601",
    (Temporal.Calendar.from(new Temporal.PlainDate(2021, 7, 3))).id);
assertEquals("iso8601",
    (Temporal.Calendar.from(new Temporal.PlainDateTime(2021, 7, 3, 4, 5))).id);
assertEquals("iso8601",
    (Temporal.Calendar.from(new Temporal.PlainTime())).id);
assertEquals("iso8601",
    (Temporal.Calendar.from(new Temporal.PlainYearMonth(2011, 4))).id);
assertEquals("iso8601",
    (Temporal.Calendar.from(new Temporal.PlainMonthDay(2, 6))).id);
