// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

let d1 = new Temporal.Duration();
let badDur = {add: d1.add};
assertThrows(() => badDur.add(d1), TypeError);

let relativeToOptions = {relativeTo: "2021-08-01"};

let d2 = new Temporal.Duration(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
assertThrows(() => d2.add(d1), RangeError);
assertThrows(() => d1.add(d2), RangeError);
assertThrows(() => d2.add(d2), RangeError);
assertDuration(d2.add(d1, relativeToOptions),
    1, 2, 0, 25, 5, 6, 7, 8, 9, 10, 1, false);
assertDuration(d1.add(d2, relativeToOptions),
    1, 2, 0, 25, 5, 6, 7, 8, 9, 10, 1, false);
assertDuration(d1.add(d1, relativeToOptions),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, true);
assertDuration(d2.add(d2, relativeToOptions),
    2, 5, 0, 19, 10, 12, 14, 16, 18, 20, 1, false);

// Test large number
let d3 = new Temporal.Duration(
    1e5, 2e5, 3e5, 4e5, 5e5, 6e5, 7e5, 8e5, 9e5, 10e5);
assertThrows(() => d3.add(d3), RangeError);
//assertDuration(d3.add(d3, relativeToOptions),
//    2e5, 4e5, 6e5, 8e5, 1e6, 12e5, 14e5, 16e5, 18e5, 2e6, 1, false);

// Test negative values
let d4 = new Temporal.Duration(-1, -2, -3, -4, -5, -6, -7, -8, -9, -10);
assertThrows(() => d4.add(d1), RangeError);
assertThrows(() => d1.add(d4), RangeError);
assertThrows(() => d4.add(d4), RangeError);
assertThrows(() => d2.add(d4), RangeError);
assertThrows(() => d4.add(d2), RangeError);
assertDuration(d4.add(d1, relativeToOptions),
    -1, -2, 0, -25, -5, -6, -7, -8, -9, -10, -1, false);
assertDuration(d1.add(d4, relativeToOptions),
    -1, -2, 0, -25, -5, -6, -7, -8, -9, -10, -1, false);
assertDuration(d4.add(d4, relativeToOptions),
    -2, -5, 0, -19, -10, -12, -14, -16, -18, -20, -1, false);
assertDuration(d2.add(d4, relativeToOptions),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, true);
assertDuration(d4.add(d2, relativeToOptions),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, true);
