// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// years in 4 digits range
assertEquals("2021-07-01T00:00:00",
    (new Temporal.PlainDateTime(2021, 7, 1)).toJSON());
assertEquals("9999-12-31T00:00:00",
    (new Temporal.PlainDateTime(9999, 12, 31)).toJSON());
assertEquals("1000-01-01T00:00:00",
    (new Temporal.PlainDateTime(1000, 1, 1)).toJSON());

// years out of 4 digits range
assertEquals("+010000-01-01T00:00:00",
    (new Temporal.PlainDateTime(10000, 1, 1)).toJSON());
assertEquals("+025021-07-01T00:00:00",
    (new Temporal.PlainDateTime(25021, 7, 1)).toJSON());
assertEquals("0999-12-31T00:00:00",
    (new Temporal.PlainDateTime(999, 12, 31)).toJSON());
assertEquals("0099-08-01T00:00:00",
    (new Temporal.PlainDateTime(99, 8, 1)).toJSON());
assertEquals("-000020-09-30T00:00:00",
    (new Temporal.PlainDateTime(-20, 9, 30)).toJSON());
assertEquals("-002021-07-01T00:00:00",
    (new Temporal.PlainDateTime(-2021, 7, 1)).toJSON());
assertEquals("-022021-07-01T00:00:00",
    (new Temporal.PlainDateTime(-22021, 7, 1)).toJSON());

// with time
assertEquals("2021-07-01T02:03:04",
    (new Temporal.PlainDateTime(2021, 7, 1, 2, 3, 4)).toJSON());
assertEquals("2021-07-01T00:03:04",
    (new Temporal.PlainDateTime(2021, 7, 1, 0, 3, 4)).toJSON());
assertEquals("2021-07-01T00:00:04",
    (new Temporal.PlainDateTime(2021, 7, 1, 0, 0, 4)).toJSON());
assertEquals("2021-07-01T00:00:00",
    (new Temporal.PlainDateTime(2021, 7, 1, 0, 0, 0)).toJSON());
assertEquals("2021-07-01T02:00:00",
    (new Temporal.PlainDateTime(2021, 7, 1, 2, 0, 0)).toJSON());
assertEquals("2021-07-01T02:03:00",
    (new Temporal.PlainDateTime(2021, 7, 1, 2, 3, 0)).toJSON());
assertEquals("2021-07-01T23:59:59",
    (new Temporal.PlainDateTime(2021, 7, 1, 23, 59, 59)).toJSON());
assertEquals("2021-07-01T00:59:59",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59)).toJSON());

assertEquals("2021-07-01T00:59:59.000000001",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 0, 0, 1)).toJSON());
assertEquals("2021-07-01T00:59:59.000008009",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 0, 8, 9)).toJSON());
assertEquals("2021-07-01T00:59:59.007008009",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 7, 8, 9)).toJSON());

assertEquals("2021-07-01T00:59:59.00000009",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 0, 0, 90)).toJSON());
assertEquals("2021-07-01T00:59:59.0000009",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 0, 0, 900)).toJSON());
assertEquals("2021-07-01T00:59:59.000008",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 0, 8)).toJSON());
assertEquals("2021-07-01T00:59:59.000008",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 0, 8, 0)).toJSON());
assertEquals("2021-07-01T00:59:59.00008",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 0, 80)).toJSON());
assertEquals("2021-07-01T00:59:59.00008",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 0, 80, 0)).toJSON());
assertEquals("2021-07-01T00:59:59.0008",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 0, 800)).toJSON());
assertEquals("2021-07-01T00:59:59.0008",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 0, 800, 0)).toJSON());
assertEquals("2021-07-01T00:59:59.007",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 7, 0, 0)).toJSON());
assertEquals("2021-07-01T00:59:59.007",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 7, 0)).toJSON());
assertEquals("2021-07-01T00:59:59.007",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 7)).toJSON());
assertEquals("2021-07-01T00:59:59.07",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 70, 0, 0)).toJSON());
assertEquals("2021-07-01T00:59:59.07",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 70, 0)).toJSON());
assertEquals("2021-07-01T00:59:59.07",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 70)).toJSON());
assertEquals("2021-07-01T00:59:59.7",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 700, 0, 0)).toJSON());
assertEquals("2021-07-01T00:59:59.7",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 700, 0)).toJSON());
assertEquals("2021-07-01T00:59:59.7",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 700)).toJSON());
assertEquals("2021-07-01T00:59:59.000876",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 0, 876)).toJSON());
assertEquals("2021-07-01T00:59:59.876",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 876)).toJSON());
assertEquals("2021-07-01T00:59:59.000000876",
    (new Temporal.PlainDateTime(2021, 7, 1, 00, 59, 59, 0, 0, 876)).toJSON());
