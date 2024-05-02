// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

assertEquals("PT0S", (new Temporal.Duration()).toJSON());

assertEquals("P1Y", (new Temporal.Duration(1)).toJSON());
assertEquals("-P1Y", (new Temporal.Duration(-1)).toJSON());
assertEquals("P1234567890Y", (new Temporal.Duration(1234567890)).toJSON());
assertEquals("-P1234567890Y", (new Temporal.Duration(-1234567890)).toJSON());

assertEquals("P1Y2M", (new Temporal.Duration(1, 2)).toJSON());
assertEquals("-P1Y2M", (new Temporal.Duration(-1, -2)).toJSON());
assertEquals("P2M", (new Temporal.Duration(0, 2)).toJSON());
assertEquals("-P2M", (new Temporal.Duration(0,-2)).toJSON());
assertEquals("P1234567890M", (new Temporal.Duration(0, 1234567890)).toJSON());
assertEquals("-P1234567890M", (new Temporal.Duration(0,-1234567890)).toJSON());

assertEquals("P1Y2M3W", (new Temporal.Duration(1, 2, 3)).toJSON());
assertEquals("-P1Y2M3W", (new Temporal.Duration(-1, -2, -3)).toJSON());
assertEquals("P3W", (new Temporal.Duration(0, 0, 3)).toJSON());
assertEquals("-P3W", (new Temporal.Duration(0, 0, -3)).toJSON());
assertEquals("P1Y3W", (new Temporal.Duration(1, 0, 3)).toJSON());
assertEquals("-P1Y3W", (new Temporal.Duration(-1, 0, -3)).toJSON());
assertEquals("P2M3W", (new Temporal.Duration(0, 2, 3)).toJSON());
assertEquals("-P2M3W", (new Temporal.Duration(0, -2, -3)).toJSON());
assertEquals("P1234567890W",
    (new Temporal.Duration(0, 0, 1234567890)).toJSON());
assertEquals("-P1234567890W",
    (new Temporal.Duration(0, 0, -1234567890)).toJSON());

assertEquals("P1Y2M3W4D", (new Temporal.Duration(1, 2, 3, 4)).toJSON());
assertEquals("-P1Y2M3W4D", (new Temporal.Duration(-1, -2, -3, -4)).toJSON());
assertEquals("P1234567890D",
    (new Temporal.Duration(0, 0, 0, 1234567890)).toJSON());
assertEquals("-P1234567890D",
    (new Temporal.Duration(0, 0, 0, -1234567890)).toJSON());
assertEquals("P4D", (new Temporal.Duration(0, 0, 0, 4)).toJSON());
assertEquals("-P4D", (new Temporal.Duration(0, 0, 0, -4)).toJSON());
assertEquals("P1Y4D", (new Temporal.Duration(1, 0, 0, 4)).toJSON());
assertEquals("-P1Y4D", (new Temporal.Duration(-1, 0, 0, -4)).toJSON());
assertEquals("P2M4D", (new Temporal.Duration(0, 2, 0, 4)).toJSON());
assertEquals("-P2M4D", (new Temporal.Duration(0, -2, 0, -4)).toJSON());
assertEquals("P3W4D", (new Temporal.Duration(0, 0, 3, 4)).toJSON());
assertEquals("-P3W4D", (new Temporal.Duration(0, 0, -3, -4)).toJSON());

assertEquals("PT5H", (new Temporal.Duration(0, 0, 0, 0, 5)).toJSON());
assertEquals("-PT5H", (new Temporal.Duration(0, 0, 0, 0, -5)).toJSON());
assertEquals("P1YT5H", (new Temporal.Duration(1, 0, 0, 0, 5)).toJSON());
assertEquals("-P1YT5H", (new Temporal.Duration(-1, 0, 0, 0, -5)).toJSON());
assertEquals("P2MT5H", (new Temporal.Duration(0, 2, 0, 0, 5)).toJSON());
assertEquals("-P2MT5H", (new Temporal.Duration(0, -2, 0, 0, -5)).toJSON());

assertEquals("PT6M", (new Temporal.Duration(0, 0, 0, 0, 0, 6)).toJSON());
assertEquals("-PT6M", (new Temporal.Duration(0, 0, 0, 0, 0, -6)).toJSON());
assertEquals("PT5H6M", (new Temporal.Duration(0, 0, 0, 0, 5, 6)).toJSON());
assertEquals("-PT5H6M", (new Temporal.Duration(0, 0, 0, 0, -5, -6)).toJSON());
assertEquals("P3WT6M", (new Temporal.Duration(0, 0, 3, 0, 0, 6)).toJSON());
assertEquals("-P3WT6M", (new Temporal.Duration(0, 0, -3, 0, 0, -6)).toJSON());
assertEquals("P4DT6M", (new Temporal.Duration(0, 0, 0, 4, 0, 6)).toJSON());
assertEquals("-P4DT6M", (new Temporal.Duration(0, 0, 0, -4, 0, -6)).toJSON());

assertEquals("PT7S", (new Temporal.Duration(0, 0, 0, 0, 0, 0, 7)).toJSON());
assertEquals("-PT7S", (new Temporal.Duration(0, 0, 0, 0, 0, 0, -7)).toJSON());
assertEquals("PT5H7S", (new Temporal.Duration(0, 0, 0, 0, 5, 0, 7)).toJSON());
assertEquals("-PT5H7S",
    (new Temporal.Duration(0, 0, 0, 0, -5, 0, -7)).toJSON());
assertEquals("PT6M7S", (new Temporal.Duration(0, 0, 0, 0, 0, 6, 7)).toJSON());
assertEquals("-PT6M7S",
    (new Temporal.Duration(0, 0, 0, 0, 0, -6, -7)).toJSON());
assertEquals("PT5H6M7S", (new Temporal.Duration(0, 0, 0, 0, 5, 6, 7)).toJSON());
assertEquals("-PT5H6M7S",
    (new Temporal.Duration(0, 0, 0, 0, -5, -6, -7)).toJSON());
assertEquals("P1YT5H6M7S",
    (new Temporal.Duration(1, 0, 0, 0, 5, 6, 7)).toJSON());
assertEquals("-P1YT5H6M7S",
    (new Temporal.Duration(-1, 0, 0, 0, -5, -6, -7)).toJSON());

assertEquals("PT0.008S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 8)).toJSON());
assertEquals("-PT0.008S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, -8)).toJSON());
assertEquals("PT0.08S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 80)).toJSON());
assertEquals("-PT0.08S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, -80)).toJSON());
assertEquals("PT0.087S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 87)).toJSON());
assertEquals("-PT0.087S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, -87)).toJSON());
assertEquals("PT0.876S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 876)).toJSON());
assertEquals("-PT0.876S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, -876)).toJSON());
assertEquals("PT876.543S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 876543)).toJSON());
assertEquals("-PT876.543S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, -876543)).toJSON());

assertEquals("PT0.000009S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 9)).toJSON());
assertEquals("-PT0.000009S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, -9)).toJSON());
assertEquals("PT0.00009S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 90)).toJSON());
assertEquals("-PT0.00009S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, -90)).toJSON());
assertEquals("PT0.000098S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 98)).toJSON());
assertEquals("-PT0.000098S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, -98)).toJSON());
assertEquals("PT0.0009S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 900)).toJSON());
assertEquals("-PT0.0009S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, -900)).toJSON());
assertEquals("PT0.000987S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 987)).toJSON());
assertEquals("-PT0.000987S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, -987)).toJSON());
assertEquals("PT0.987654S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 987654)).toJSON());
assertEquals("-PT0.987654S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, -987654)).toJSON());
assertEquals("PT987.654321S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 987654321)).toJSON());
assertEquals("-PT987.654321S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, -987654321)).toJSON());

assertEquals("PT0.000000001S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 1)).toJSON());
assertEquals("-PT0.000000001S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, -1)).toJSON());
assertEquals("PT0.00000001S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 10)).toJSON());
assertEquals("-PT0.00000001S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, -10)).toJSON());
assertEquals("PT0.000000012S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 12)).toJSON());
assertEquals("-PT0.000000012S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, -12)).toJSON());
assertEquals("PT0.0000001S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 100)).toJSON());
assertEquals("-PT0.0000001S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, -100)).toJSON());
assertEquals("PT0.000000123S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 123)).toJSON());
assertEquals("-PT0.000000123S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, -123)).toJSON());
assertEquals("PT0.000123456S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 123456)).toJSON());
assertEquals("-PT0.000123456S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, -123456)).toJSON());
assertEquals("PT0.123456789S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 123456789)).toJSON());
assertEquals("-PT0.123456789S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, -123456789)).toJSON());
assertEquals("PT1.234567891S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 1234567891)).toJSON());
assertEquals("-PT1.234567891S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, -1234567891)).toJSON());
assertEquals("PT4.003002001S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 4, 3, 2, 1)).toJSON());
assertEquals("-PT4.003002001S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, -4, -3, -2, -1)).toJSON());
assertEquals("PT4.003092001S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 4, 3, 2, 90001)).toJSON());
assertEquals("-PT4.003092001S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, -4, -3, -2, -90001)).toJSON());
assertEquals("PT4.093082001S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 4, 3, 2, 90080001)).toJSON());
assertEquals("-PT4.093082001S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, -4, -3, -2, -90080001)).toJSON());

assertEquals("P1Y2M3W4DT5H6M7.008009001S",
    (new Temporal.Duration(1, 2, 3, 4, 5, 6, 7, 8, 9, 1)).toJSON());
assertEquals("-P1Y2M3W4DT5H6M7.008009001S",
    (new Temporal.Duration(-1, -2, -3, -4, -5, -6, -7, -8, -9, -1)).toJSON());
assertEquals("P1234Y2345M3456W4567DT5678H6789M7890.890901123S",
    (new Temporal.Duration(1234, 2345, 3456, 4567, 5678, 6789, 7890,
                           890, 901, 123)).toJSON());
assertEquals("-P1234Y2345M3456W4567DT5678H6789M7890.890901123S",
    (new Temporal.Duration(-1234, -2345, -3456, -4567, -5678, -6789, -7890,
                           -890, -901, -123)).toJSON());
