// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// Test Temporal.Duration toJSON with extreme cases.
//
// Test Number.MAX_VALUE
//
// This is out of the range of Number.MAX_SAFE_INTEGER so the specification
// does not mandate the precision. But we should still check certain property of
// the result.
// Number.MAX_VALUE is 1.7976931348623157e+308 so the first 16 characters should
// be "P179769313486231" which is 15 digits and only require 50 bits so that
// should be precious in 64 bit floating point.
// There are total 309 digits so it should be 179769313486231 with another
// 294 digits (309-15 = 294)
assertMatches(/P179769313486231\d{294}Y/,
    (new Temporal.Duration(Number.MAX_VALUE)).toJSON());
assertMatches(/-P179769313486231\d{294}Y/,
    (new Temporal.Duration(-Number.MAX_VALUE)).toJSON());

assertMatches(/P179769313486231\d{294}M/,
    (new Temporal.Duration(0, Number.MAX_VALUE)).toJSON());
assertMatches(/-P179769313486231\d{294}M/,
    (new Temporal.Duration(0, -Number.MAX_VALUE)).toJSON());

assertMatches(/P179769313486231\d{294}W/,
    (new Temporal.Duration(0, 0, Number.MAX_VALUE)).toJSON());
assertMatches(/-P179769313486231\d{294}W/,
    (new Temporal.Duration(0, 0, -Number.MAX_VALUE)).toJSON());

assertMatches(/P179769313486231\d{294}D/,
    (new Temporal.Duration(0, 0, 0, Number.MAX_VALUE)).toJSON());
assertMatches(/-P179769313486231\d{294}D/,
    (new Temporal.Duration(0, 0, 0, -Number.MAX_VALUE)).toJSON());

assertMatches(/PT179769313486231\d{294}H/,
    (new Temporal.Duration(0, 0, 0, 0, Number.MAX_VALUE)).toJSON());
assertMatches(/-PT179769313486231\d{294}H/,
    (new Temporal.Duration(0, 0, 0, 0, -Number.MAX_VALUE)).toJSON());

assertMatches(/PT179769313486231\d{294}M/,
    (new Temporal.Duration(0, 0, 0, 0, 0, Number.MAX_VALUE)).toJSON());
assertMatches(/-PT179769313486231\d{294}M/,
    (new Temporal.Duration(0, 0, 0, 0, 0, -Number.MAX_VALUE)).toJSON());

assertMatches(/PT179769313486231\d{294}S/,
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, Number.MAX_VALUE)).toJSON());
assertMatches(/-PT179769313486231\d{294}S/,
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, -Number.MAX_VALUE)).toJSON());

// For milliseconds, we should have 179769313486231 with another 291
// (309 - 15 - 3 = 291) digits, a '.', and then 3 digits
assertMatches(/PT179769313486231\d{291}[.]\d{3}S/,
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, Number.MAX_VALUE)).toJSON());
assertMatches(/-PT179769313486231\d{291}[.]\d{3}S/,
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, -Number.MAX_VALUE)).toJSON());

// For microseconds, we should have 179769313486231 with another 288
// (309 - 15 - 6 = 288) digits, a '.', and then 6 digits
assertMatches(/PT179769313486231\d{288}[.]\d{6}S/,
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, Number.MAX_VALUE))
    .toJSON());
assertMatches(/-PT179769313486231\d{288}[.]\d{6}S/,
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, -Number.MAX_VALUE))
    .toJSON());

// For nanoseconds, we should have 179769313486231 with another 285
// (309 - 15 - 9 = 285) digits, a '.', and then 9 digits
assertMatches(/PT179769313486231\d{285}[.]\d{9}S/,
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, Number.MAX_VALUE))
    .toJSON());
assertMatches(/-PT179769313486231\d{285}[.]\d{9}S/,
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, -Number.MAX_VALUE))
    .toJSON());

// Test seconds + milliseconds
// Number.MAX_VALUE + Number.MAX_VALUE / 1000 = 1.7994908279971777e+308
// So the first 17 characters should be "PT179949082799717" which is 15 digits
// and only require 50 bits so that should be precious in 64 bit floating point.
// For seconds and milliseconds, we should have 179949082799717 with another 294
// digits, a '.', and then 3 digits
assertMatches(/PT179949082799717\d{294}[.]\d{3}S/,
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, Number.MAX_VALUE,
                           Number.MAX_VALUE)).toJSON());
assertMatches(/-PT179949082799717\d{294}[.]\d{3}S/,
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, -Number.MAX_VALUE,
                           -Number.MAX_VALUE)).toJSON());

// Test milliseconds + microseconds
// For milliseconds and microseconds, we should have 179949082799717 with
// another 291 (309 - 15 - 3 = 291) digits, a '.', and then 6 digits.
assertMatches(/PT179949082799717\d{291}[.]\d{6}S/,
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, Number.MAX_VALUE,
                           Number.MAX_VALUE)).toJSON());
assertMatches(/-PT179949082799717\d{291}[.]\d{6}S/,
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, -Number.MAX_VALUE,
                           -Number.MAX_VALUE)).toJSON());

// Test microseconds + nanoseconds
// For microseconds and nanoseconds, we should have 179949082799717 with another
// 288 (309 - 15 - 6 = 288) digits, a '.', and then 9 digits.
assertMatches(/PT179949082799717\d{288}[.]\d{9}S/,
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, Number.MAX_VALUE,
                           Number.MAX_VALUE)).toJSON());
assertMatches(/-PT179949082799717\d{288}[.]\d{9}S/,
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, -Number.MAX_VALUE,
                           -Number.MAX_VALUE)).toJSON());

// Test seconds + milliseconds + microseconds
// Number.MAX_VALUE + Number.MAX_VALUE / 1000 + Number.MAX_VALUE / 1000000 =
// 1.7994926256903124e+308
// So the first 17 characters should be "PT179949262569031" which is 15 digits
// and only require 50 bits so that should be precious in 64 bit floating point.
// For seconds and milliseconds and microseconds, we should have 179949262569031
// with another 294 digits, a '.', and then 6 digits.
assertMatches(/PT179949262569031\d{294}[.]\d{6}S/,
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, Number.MAX_VALUE,
                           Number.MAX_VALUE, Number.MAX_VALUE)).toJSON());
assertMatches(/-PT179949262569031\d{294}[.]\d{6}S/,
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, -Number.MAX_VALUE,
                           -Number.MAX_VALUE, -Number.MAX_VALUE)).toJSON());

// Test milliseconds + microseconds + nanoseconds
// For milliseconds and microseconds and nanoseconds, we should have
// 179949262569031 with another 291 digits, a '.', and then 9 digits.
assertMatches(/PT179949262569031\d{291}[.]\d{9}S/,
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, Number.MAX_VALUE,
                           Number.MAX_VALUE, Number.MAX_VALUE)).toJSON());
assertMatches(/-PT179949262569031\d{291}[.]\d{9}S/,
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, -Number.MAX_VALUE,
                           -Number.MAX_VALUE, -Number.MAX_VALUE)).toJSON());

// Test seconds + milliseconds + microseconds + nanoseconds
// Number.MAX_VALUE + Number.MAX_VALUE / 1000 + Number.MAX_VALUE / 1000000 +
// Number.MAX_VALUE / 1000000000 = 1.7994926274880055e+308
// So the first 17 characters should be "PT179949262748800" which is 15 digits
// and only require 50 bits so that should be precious in 64 bit floating point.
// For seconds and milliseconds and microseconds, and nanoseconds, we should
// have 179949262748800 with another 294 digits, a '.', and then 9 digits.
assertMatches(/PT179949262748800\d{294}[.]\d{9}S/,
    (new Temporal.Duration(
        0, 0, 0, 0, 0, 0, Number.MAX_VALUE, Number.MAX_VALUE, Number.MAX_VALUE,
        Number.MAX_VALUE)).toJSON());
assertMatches(/-PT179949262748800\d{294}[.]\d{9}S/,
    (new Temporal.Duration(
        0, 0, 0, 0, 0, 0, -Number.MAX_VALUE, -Number.MAX_VALUE,
        -Number.MAX_VALUE, -Number.MAX_VALUE)).toJSON());

// Test Number.MAX_SAFE_INTEGER
// For MAX_SAFE_INTEGER, we need to test the result come out as exact, not just
// close.
let maxIntString = String(Number.MAX_SAFE_INTEGER);

assertEquals("P" + maxIntString + "Y",
    (new Temporal.Duration(Number.MAX_SAFE_INTEGER)).toJSON());
assertEquals("-P" + maxIntString + "Y",
    (new Temporal.Duration(-Number.MAX_SAFE_INTEGER)).toJSON());

assertEquals("P" + maxIntString + "M",
    (new Temporal.Duration(0, Number.MAX_SAFE_INTEGER)).toJSON());
assertEquals("-P" + maxIntString + "M",
    (new Temporal.Duration(0, -Number.MAX_SAFE_INTEGER)).toJSON());

assertEquals("P" + maxIntString + "W",
    (new Temporal.Duration(0, 0, Number.MAX_SAFE_INTEGER)).toJSON());
assertEquals("-P" + maxIntString + "W",
    (new Temporal.Duration(0, 0, -Number.MAX_SAFE_INTEGER)).toJSON());

assertEquals("P" + maxIntString + "D",
    (new Temporal.Duration(0, 0, 0, Number.MAX_SAFE_INTEGER)).toJSON());
assertEquals("-P" + maxIntString + "D",
    (new Temporal.Duration(0, 0, 0, -Number.MAX_SAFE_INTEGER)).toJSON());

assertEquals("PT" + maxIntString + "H",
    (new Temporal.Duration(0, 0, 0, 0, Number.MAX_SAFE_INTEGER)).toJSON());
assertEquals("-PT" + maxIntString + "H",
    (new Temporal.Duration(0, 0, 0, 0, -Number.MAX_SAFE_INTEGER)).toJSON());

assertEquals("PT" + maxIntString + "M",
    (new Temporal.Duration(0, 0, 0, 0, 0, Number.MAX_SAFE_INTEGER)).toJSON());
assertEquals("-PT" + maxIntString + "M",
    (new Temporal.Duration(0, 0, 0, 0, 0, -Number.MAX_SAFE_INTEGER)).toJSON());

assertEquals("PT" + maxIntString + "S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, Number.MAX_SAFE_INTEGER))
    .toJSON());
assertEquals("-PT" + maxIntString + "S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, -Number.MAX_SAFE_INTEGER))
    .toJSON());

const insertDotFromRight = (str, pos) =>
    `${str.slice(0, str.length-pos)}.${str.slice(str.length-pos)}`;

// For milliseconds, microseconds, and nanoseconds
assertEquals("PT" + insertDotFromRight(maxIntString, 3) + "S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, Number.MAX_SAFE_INTEGER))
    .toJSON());
assertEquals("-PT" + insertDotFromRight(maxIntString, 3) + "S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, -Number.MAX_SAFE_INTEGER))
    .toJSON());

assertEquals("PT" + insertDotFromRight(maxIntString, 6) + "S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, Number.MAX_SAFE_INTEGER))
    .toJSON());
assertEquals("-PT" + insertDotFromRight(maxIntString, 6) + "S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, -Number.MAX_SAFE_INTEGER))
    .toJSON());

assertEquals("PT" + insertDotFromRight(maxIntString, 9) + "S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, Number.MAX_SAFE_INTEGER))
    .toJSON());
assertEquals("-PT" + insertDotFromRight(maxIntString, 9) + "S",
    (new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, -Number.MAX_SAFE_INTEGER))
    .toJSON());

// Test seconds + milliseconds
// Number.MAX_SAFE_INTEGER: 9007199254740991
//   9007199254740991
//      9007199254740.991
//_+_____________________
//   9016206453995731.991
let twoMaxString = "9016206453995731991";
assertEquals("PT" + insertDotFromRight(twoMaxString, 3) + "S",
    (new Temporal.Duration(
        0, 0, 0, 0, 0, 0, Number.MAX_SAFE_INTEGER, Number.MAX_SAFE_INTEGER))
    .toJSON());
assertEquals("-PT" + insertDotFromRight(twoMaxString, 3) + "S",
    (new Temporal.Duration(
        0, 0, 0, 0, 0, 0, -Number.MAX_SAFE_INTEGER, -Number.MAX_SAFE_INTEGER))
    .toJSON());

// Test milliseconds + microseconds
assertEquals("PT" + insertDotFromRight(twoMaxString, 6) + "S",
    (new Temporal.Duration(
        0, 0, 0, 0, 0, 0, 0, Number.MAX_SAFE_INTEGER, Number.MAX_SAFE_INTEGER))
    .toJSON());
assertEquals("-PT" + insertDotFromRight(twoMaxString, 6) + "S",
    (new Temporal.Duration(
        0, 0, 0, 0, 0, 0, 0, -Number.MAX_SAFE_INTEGER, -Number.MAX_SAFE_INTEGER)
     ).toJSON());

// Test microseconds + nanoseconds
assertEquals("PT" + insertDotFromRight(twoMaxString, 9) + "S",
    (new Temporal.Duration(
        0, 0, 0, 0, 0, 0, 0, 0, Number.MAX_SAFE_INTEGER,
        Number.MAX_SAFE_INTEGER)).toJSON());
assertEquals("-PT" + insertDotFromRight(twoMaxString, 9) + "S",
    (new Temporal.Duration(
        0, 0, 0, 0, 0, 0, 0, 0, -Number.MAX_SAFE_INTEGER,
        -Number.MAX_SAFE_INTEGER)).toJSON());

// Test seconds + milliseconds + microseconds
// Number.MAX_SAFE_INTEGER: 9007199254740991
//   9007199254740991
//      9007199254740.991
//         9007199254.740991
//_+_____________________
//   9016215461194986.731991
let threeMaxString = "9016215461194986731991";
assertEquals("PT" + insertDotFromRight(threeMaxString, 6) + "S",
    (new Temporal.Duration(
        0, 0, 0, 0, 0, 0, Number.MAX_SAFE_INTEGER, Number.MAX_SAFE_INTEGER,
        Number.MAX_SAFE_INTEGER)).toJSON());
assertEquals("-PT" + insertDotFromRight(threeMaxString, 6) + "S",
    (new Temporal.Duration(
        0, 0, 0, 0, 0, 0, -Number.MAX_SAFE_INTEGER, -Number.MAX_SAFE_INTEGER,
        -Number.MAX_SAFE_INTEGER)).toJSON());

// Test milliseconds + microseconds + nanoseconds
assertEquals("PT" + insertDotFromRight(threeMaxString, 9) + "S",
    (new Temporal.Duration(
        0, 0, 0, 0, 0, 0, 0, Number.MAX_SAFE_INTEGER, Number.MAX_SAFE_INTEGER,
        Number.MAX_SAFE_INTEGER)).toJSON());
assertEquals("-PT" + insertDotFromRight(threeMaxString, 9) + "S",
    (new Temporal.Duration(
        0, 0, 0, 0, 0, 0, 0, -Number.MAX_SAFE_INTEGER, -Number.MAX_SAFE_INTEGER,
        -Number.MAX_SAFE_INTEGER)).toJSON());

// Test seconds + milliseconds + microseconds + nanoseconds
// Number.MAX_SAFE_INTEGER: 9007199254740991
//   9007199254740991
//      9007199254740.991
//         9007199254.740991
//            9007199.254740991
//_+____________________________
//   9016215470202185.986731991
let fourMaxString = "9016215470202185.986731991";
assertEquals("PT" + fourMaxString + "S",
    (new Temporal.Duration(
        0, 0, 0, 0, 0, 0, Number.MAX_SAFE_INTEGER, Number.MAX_SAFE_INTEGER,
        Number.MAX_SAFE_INTEGER, Number.MAX_SAFE_INTEGER)).toJSON());
assertEquals("-PT" + fourMaxString + "S",
    (new Temporal.Duration(
        0, 0, 0, 0, 0, 0, -Number.MAX_SAFE_INTEGER, -Number.MAX_SAFE_INTEGER,
        -Number.MAX_SAFE_INTEGER, -Number.MAX_SAFE_INTEGER)).toJSON());
