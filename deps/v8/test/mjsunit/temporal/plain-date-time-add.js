// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

// Simple add
assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 1,2,3,4,5,6))
    .add("PT9H8M7.080090010S"), 2021, 7, 20, 10, 10, 10, 84, 95, 16);

assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 0,0,0,0,1,996))
    .add("PT0.0000071S"), 2021, 7, 20, 0, 0, 0, 0, 9, 96);

assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 0,0,0,1,996))
    .add("PT0.0071S"), 2021, 7, 20, 0, 0, 0, 9, 96, 0);

assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 0,0,1,996))
    .add("PT7.1S"), 2021, 7, 20, 0, 0, 9, 96, 0, 0, 0);

assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 0,1,59))
    .add("PT5M7S"), 2021, 7, 20, 0, 7, 6, 0, 0, 0);

assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 1,59))
    .add("PT5H7M"), 2021, 7, 20, 7, 6, 0, 0, 0, 0);

assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 19))
    .add("PT8H"),  2021, 7, 21, 3, 0, 0, 0, 0, 0);

assertPlainDateTime(
    (new Temporal.PlainDateTime(2021, 7, 20, 21,52,53,994,995,996))
    .add("PT5H13M11.404303202S"), 2021, 7, 21, 3, 6, 5, 399, 299, 198);

assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 0,0,0,0,0,995))
    .add("PT0.000000006S"), 2021, 7, 20, 0, 0, 0, 0, 1, 1);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 0,0,0,0,0,995))
    .add("PT0.00000006S"), 2021, 7, 20, 0, 0, 0, 0, 1, 55);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 0,0,0,0,0,995))
    .add("PT0.0000006S"), 2021, 7, 20, 0, 0, 0, 0, 1, 595);

assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 1,2,3,4,5,6))
    .add("-PT0.000000007S"), 2021, 7, 20, 1, 2, 3, 4, 4, 999);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 1,2,3,4,5,6))
    .add("-PT0.000005007S"), 2021, 7, 20, 1, 2, 3, 3, 999, 999);

assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 1,2,3,4,5,6))
    .add("-PT0.004005007S"), 2021, 7, 20, 1, 2, 2, 999, 999, 999);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 1,2,3,4,5,6))
    .add("-PT0.005006007S"), 2021, 7, 20, 1, 2, 2, 998, 998, 999);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 1,2,3,4,5,6))
    .add("-PT4.005006007S"), 2021, 7, 20, 1, 1, 58, 998, 998, 999);

assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 1,2,3,4,5,6))
    .add("-PT4S"), 2021, 7, 20, 1, 1, 59, 4, 5, 6);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 1,2,3,4,5,6))
    .add("-PT5M"), 2021, 7, 20, 0, 57, 3, 4, 5, 6);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 1,2,3,4,5,6))
    .add("-PT1H5M"), 2021, 7, 19, 23, 57, 3, 4, 5, 6);
assertPlainDateTime((new Temporal.PlainDateTime(2021, 7, 20, 1,2,3,4,5,6))
    .add("-PT1H5M4S"), 2021, 7, 19, 23, 56, 59, 4, 5, 6);

let goodDateTime = new Temporal.PlainDateTime(2021, 7, 20, 1,2,3);
let badDateTime = {add: goodDateTime.add};
assertThrows(() => badDateTime.add("PT30M"), TypeError);

// Throw in ToLimitedTemporalDuration
assertThrows(() => (new Temporal.PlainDateTime(2021, 7, 20, 1,2,3))
    .add("bad duration"), RangeError);
