// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

let d1 = new Temporal.Duration(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
assertDuration(d1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1, false);
let d2 = Temporal.Duration.from(d1);
assertDuration(d2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1, false);
assertNotSame(d1, d2);

assertDuration(Temporal.Duration.from("PT0S"),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, true);
assertDuration(Temporal.Duration.from("P1Y"),
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("P2M"),
    0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("P3W"),
    0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("P4D"),
    0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT5H"),
    0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT6M"),
    0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT7S"),
    0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT0.008S"),
    0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT0.000009S"),
    0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 1, false);
assertDuration(Temporal.Duration.from("PT0.000000001S"),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, false);
assertDuration(Temporal.Duration.from("P1Y2M3W4DT5H6M7.008009001S"),
    1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 1, false);
assertDuration(Temporal.Duration.from(
    "P111111111Y222222222M333333333W444444444D" +
    "T555555555H666666666M777777777.987654321S"),
    111111111, 222222222, 333333333, 444444444,
    555555555, 666666666, 777777777, 987, 654, 321, 1, false);

assertDuration(Temporal.Duration.from("P1Y3WT5H7.000009001S"),
    1, 0, 3, 0, 5, 0, 7, 0, 9, 1, 1, false);
assertDuration(Temporal.Duration.from("P2M4DT6M0.008000001S"),
    0, 2, 0, 4, 0, 6, 0, 8, 0, 1, 1, false);
assertDuration(Temporal.Duration.from("P1Y4DT7.000000001S"),
    1, 0, 0, 4, 0, 0, 7, 0, 0, 1, 1, false);
assertDuration(Temporal.Duration.from("P2MT5H0.008S"),
    0, 2, 0, 0, 5, 0, 0, 8, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("P3WT6M0.000009S"),
    0, 0, 3, 0, 0, 6, 0, 0, 9, 0, 1, false);
assertDuration(Temporal.Duration.from("P1YT5H0.000009000S"),
    1, 0, 0, 0, 5, 0, 0, 0, 9, 0, 1, false);
assertDuration(Temporal.Duration.from("P2MT6M0.000000001S"),
    0, 2, 0, 0, 0, 6, 0, 0, 0, 1, 1, false);
assertDuration(Temporal.Duration.from("P3WT7S"),
    0, 0, 3, 0, 0, 0, 7, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("P4DT0.008S"),
    0, 0, 0, 4, 0, 0, 0, 8, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("P1YT5H0.000000001S"),
    1, 0, 0, 0, 5, 0, 0, 0, 0, 1, 1, false);
assertDuration(Temporal.Duration.from("P2MT6M"),
    0, 2, 0, 0, 0, 6, 0, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("P3WT7S"),
    0, 0, 3, 0, 0, 0, 7, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("P4DT0.008S"),
    0, 0, 0, 4, 0, 0, 0, 8, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT5H0.000009S"),
    0, 0, 0, 0, 5, 0, 0, 0, 9, 0, 1, false);
assertDuration(Temporal.Duration.from("P1YT6M"),
    1, 0, 0, 0, 0, 6, 0, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("P2MT7S"),
    0, 2, 0, 0, 0, 0, 7, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("P3WT0.008S"),
    0, 0, 3, 0, 0, 0, 0, 8, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("P4DT0.000009S"),
    0, 0, 0, 4, 0, 0, 0, 0, 9, 0, 1, false);
assertDuration(Temporal.Duration.from("PT5H0.000000001S"),
    0, 0, 0, 0, 5, 0, 0, 0, 0, 1, 1, false);
assertDuration(Temporal.Duration.from("P1YT7S"),
    1, 0, 0, 0, 0, 0, 7, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("P2MT0.008S"),
    0, 2, 0, 0, 0, 0, 0, 8, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("P3WT0.000009S"),
    0, 0, 3, 0, 0, 0, 0, 0, 9, 0, 1, false);
assertDuration(Temporal.Duration.from("P4DT0.000000001S"),
    0, 0, 0, 4, 0, 0, 0, 0, 0, 1, 1, false);
assertDuration(Temporal.Duration.from("PT5H"),
    0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 1, false);

assertDuration(Temporal.Duration.from("-P1Y2M3W4DT5H6M7.008009001S"),
    -1, -2, -3, -4, -5, -6, -7, -8, -9, -1, -1, false);
assertDuration(Temporal.Duration.from(
    "-P111111111Y222222222M333333333W444444444D" +
    "T555555555H666666666M777777777.987654321S"),
    -111111111, -222222222, -333333333, -444444444,
    -555555555, -666666666, -777777777, -987, -654, -321, -1, false);
// \\u2212
assertDuration(Temporal.Duration.from("\u2212P1Y2M3W4DT5H6M7.008009001S"),
    -1, -2, -3, -4, -5, -6, -7, -8, -9, -1, -1, false);
// positive sign
assertDuration(Temporal.Duration.from("+P1Y2M3W4DT5H6M7.008009001S"),
    1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 1, false);

assertDuration(Temporal.Duration.from("PT2.5H"),
    0, 0, 0, 0, 2, 30, 0, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT2.25H"),
    0, 0, 0, 0, 2, 15, 0, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT2.05H"),
    0, 0, 0, 0, 2, 03, 0, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT2.005H"),
    0, 0, 0, 0, 2, 0, 18, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT2.505H"),
    0, 0, 0, 0, 2, 30, 18, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT2.0025H"),
    0, 0, 0, 0, 2, 0, 9, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT3.5M"),
    0, 0, 0, 0, 0, 3, 30, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT3.25M"),
    0, 0, 0, 0, 0, 3, 15, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT3.125M"),
    0, 0, 0, 0, 0, 3, 7, 500, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT3.025M"),
    0, 0, 0, 0, 0, 3, 1, 500, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT3.01M"),
    0, 0, 0, 0, 0, 3, 0, 600, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT3.005M"),
    0, 0, 0, 0, 0, 3, 0, 300, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT3.001M"),
    0, 0, 0, 0, 0, 3, 0, 60, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT3.006M"),
    0, 0, 0, 0, 0, 3, 0, 360, 0, 0, 1, false);

// Use , instead of .
assertDuration(Temporal.Duration.from("PT2,5H"),
    0, 0, 0, 0, 2, 30, 0, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT2,25H"),
    0, 0, 0, 0, 2, 15, 0, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT2,05H"),
    0, 0, 0, 0, 2, 03, 0, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT2,005H"),
    0, 0, 0, 0, 2, 0, 18, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT2,505H"),
    0, 0, 0, 0, 2, 30, 18, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT2,0025H"),
    0, 0, 0, 0, 2, 0, 9, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT3,5M"),
    0, 0, 0, 0, 0, 3, 30, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT3,25M"),
    0, 0, 0, 0, 0, 3, 15, 0, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT3,125M"),
    0, 0, 0, 0, 0, 3, 7, 500, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT3,025M"),
    0, 0, 0, 0, 0, 3, 1, 500, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT3,01M"),
    0, 0, 0, 0, 0, 3, 0, 600, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT3,005M"),
    0, 0, 0, 0, 0, 3, 0, 300, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT3,001M"),
    0, 0, 0, 0, 0, 3, 0, 60, 0, 0, 1, false);
assertDuration(Temporal.Duration.from("PT3,006M"),
    0, 0, 0, 0, 0, 3, 0, 360, 0, 0, 1, false);

assertThrows(() => Temporal.Duration.from("P2H"), RangeError);
assertThrows(() => Temporal.Duration.from("P2.5M"), RangeError);
assertThrows(() => Temporal.Duration.from("P2,5M"), RangeError);
assertThrows(() => Temporal.Duration.from("P2S"), RangeError);
assertThrows(() => Temporal.Duration.from("PT2.H3M"), RangeError);
assertThrows(() => Temporal.Duration.from("PT2,H3M"), RangeError);
assertThrows(() => Temporal.Duration.from("PT2.H3S"), RangeError);
assertThrows(() => Temporal.Duration.from("PT2,H3S"), RangeError);
assertThrows(() => Temporal.Duration.from("PT2.H0.5M"), RangeError);
assertThrows(() => Temporal.Duration.from("PT2,H0,5M"), RangeError);
assertThrows(() => Temporal.Duration.from("PT2.H0.5S"), RangeError);
assertThrows(() => Temporal.Duration.from("PT2,H0,5S"), RangeError);
assertThrows(() => Temporal.Duration.from("PT2H3.2M3S"), RangeError);
assertThrows(() => Temporal.Duration.from("PT2H3,2M3S"), RangeError);
assertThrows(() => Temporal.Duration.from("PT2H3.2M0.3S"), RangeError);
assertThrows(() => Temporal.Duration.from("PT2H3,2M0,3S"), RangeError);
assertThrows(() => Temporal.Duration.from("PT.1H"), RangeError);
assertThrows(() => Temporal.Duration.from("PT,1H"), RangeError);
assertThrows(() => Temporal.Duration.from("PT.1M"), RangeError);
assertThrows(() => Temporal.Duration.from("PT,1M"), RangeError);
assertThrows(() => Temporal.Duration.from("PT.1S"), RangeError);
assertThrows(() => Temporal.Duration.from("PT,1S"), RangeError);

assertDuration(Temporal.Duration.from(
    {years: 0, months: 0, weeks: 0, days: 0,
      hours: 0, minutes: 0, seconds: 0,
    milliseconds: 0, microseconds: 0, nanoseconds:0}),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, true);

assertDuration(Temporal.Duration.from(
    {years: 1, months: 2, weeks: 3, days: 4,
      hours: 5, minutes: 6, seconds: 7,
    milliseconds: 8, microseconds: 9, nanoseconds:10}),
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1, false);

assertDuration(Temporal.Duration.from(
    {years: -1, months: -2, weeks: -3, days: -4,
      hours: -5, minutes: -6, seconds: -7,
    milliseconds: -8, microseconds: -9, nanoseconds:-10}),
    -1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -1, false);
