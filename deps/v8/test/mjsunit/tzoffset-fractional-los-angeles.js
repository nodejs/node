// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --icu-timezone-data --allow-natives-syntax
// Environment Variables: TZ=America/Los_Angeles

// Before time zones were standardized, Los Angeles used local mean time,
// UTC-07:52:58. getTimezoneOffset() must not truncate that to whole minutes.
// See https://crbug.com/40814804.

const kLmtOffsetMs = 7 * 3600000 + 52 * 60000 + 58 * 1000;  // 28378000
const kLmtOffsetMin = kLmtOffsetMs / 60000;                 // 472.9666...

const lmt = new Date(Date.UTC(1675, 0, 1));
assertEquals(kLmtOffsetMin, lmt.getTimezoneOffset());
assertFalse(Number.isInteger(lmt.getTimezoneOffset()));

// The point of the bug report: shifting a date by its own offset must make the
// local fields read back as the original UTC fields. Truncating the offset to
// 472 minutes lands on 1674-12-31 23:59:02 instead.
const shifted = new Date(lmt.valueOf() + lmt.getTimezoneOffset() * 60000);
assertEquals(1675, shifted.getFullYear());
assertEquals(0, shifted.getMonth());
assertEquals(1, shifted.getDate());
assertEquals(0, shifted.getHours());
assertEquals(0, shifted.getMinutes());
assertEquals(0, shifted.getSeconds());

// The local time itself already had second precision, so it must agree with
// the offset.
assertEquals(16, lmt.getHours());
assertEquals(7, lmt.getMinutes());
assertEquals(2, lmt.getSeconds());

// toString() keeps minute precision, per TimeZoneString().
// https://tc39.es/ecma262/#sec-timezoneestring
assertTrue(lmt.toString().includes('GMT-0752'), lmt.toString());

// Whole-minute offsets still produce Smis, and PST/PDT are unaffected.
const pst = new Date(Date.UTC(2020, 0, 1));
const pdt = new Date(Date.UTC(2020, 6, 1));
assertEquals(480, pst.getTimezoneOffset());
assertEquals(420, pdt.getTimezoneOffset());
assertTrue(%IsSmi(pst.getTimezoneOffset()));

// NaN dates still report NaN.
assertEquals(NaN, new Date(NaN).getTimezoneOffset());

// Non-Date receivers still throw.
assertThrows(() => Date.prototype.getTimezoneOffset.call({}), TypeError);
assertThrows(() => Date.prototype.getTimezoneOffset.call(undefined), TypeError);

// Same results after the builtin is optimized.
function offsetOf(d) { return d.getTimezoneOffset(); }
%PrepareFunctionForOptimization(offsetOf);
assertEquals(kLmtOffsetMin, offsetOf(lmt));
assertEquals(480, offsetOf(pst));
%OptimizeFunctionOnNextCall(offsetOf);
assertEquals(kLmtOffsetMin, offsetOf(lmt));
assertEquals(480, offsetOf(pst));
assertEquals(NaN, offsetOf(new Date(NaN)));
