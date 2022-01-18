// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

let d1 = new Temporal.PlainDateTime(2021, 2, 28, 11, 12, 13);
let d2 = Temporal.PlainDateTime.from(
    {year: 2021, month: 2, day: 28, hour:11, minute:12, second:13});
let d3 = Temporal.PlainDateTime.from(
    {year: 2021, month: 2, day: 28, hour:11, minute:12, second:13,
      nanosecond:1});

assertEquals(d1.equals(d2), true);
assertEquals(d1.equals(d3), false);
assertEquals(d2.equals(d3), false);

let badDate = {equals: d1.equals};
assertThrows(() => badDate.equals(d1), TypeError);
