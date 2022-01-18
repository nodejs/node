// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

let t1 = new Temporal.PlainDateTime(2021, 3, 14, 1, 2, 3, 4, 5, 6);
let t2 = new Temporal.PlainDateTime(2021, 3, 14, 1, 2, 3, 4, 5, 6);
let t3 = t1;
let t4 = new Temporal.PlainDateTime(2021, 3, 15, 1, 2, 3, 4, 5, 6);
let t5 = new Temporal.PlainDateTime(2021, 4, 14, 1, 2, 3, 4, 5, 6);
let t6 = new Temporal.PlainDateTime(2022, 3, 14, 1, 2, 3, 4, 5, 6);
let t7 = new Temporal.PlainDateTime(2021, 3, 14, 1, 2, 3, 4, 5, 7);
// years in 4 digits range
assertEquals(0, Temporal.PlainDateTime.compare(t1, t1));
assertEquals(0, Temporal.PlainDateTime.compare(t1, t2));
assertEquals(0, Temporal.PlainDateTime.compare(t1, t3));
assertEquals(1, Temporal.PlainDateTime.compare(t1, "2021-03-14T01:02:03"));
assertEquals(1, Temporal.PlainDateTime.compare(t4, t1));
assertEquals(1, Temporal.PlainDateTime.compare(t5, t1));
assertEquals(1, Temporal.PlainDateTime.compare(t6, t1));
assertEquals(1, Temporal.PlainDateTime.compare(t7, t1));
assertEquals(-1, Temporal.PlainDateTime.compare(t1, t4));
assertEquals(-1, Temporal.PlainDateTime.compare(t1, t5));
assertEquals(-1, Temporal.PlainDateTime.compare(t1, t6));
assertEquals(-1, Temporal.PlainDateTime.compare(t1, t7));
assertEquals(1, Temporal.PlainDateTime.compare("2021-07-21", t1));
assertEquals(0, Temporal.PlainDateTime.compare(
    t1, "2021-03-14T01:02:03.004005006"));

// Test Throw
assertThrows(() => Temporal.PlainDateTime.compare(t1, "invalid iso8601 string"),
    RangeError);
assertThrows(() => Temporal.PlainDateTime.compare("invalid iso8601 string", t1),
    RangeError);
