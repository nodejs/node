// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

let d1 = new Temporal.Duration();
assertDuration(d1.negated(), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, true);

let d2 = new Temporal.Duration(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
assertDuration(d2.negated(), -1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -1, false);

// Test large number
let d3 = new Temporal.Duration(
    1e5, 2e5, 3e5, 4e5, 5e5, 6e5, 7e5, 8e5, 9e5, 10e5);
assertDuration(d3.negated(),
    -1e5, -2e5, -3e5, -4e5, -5e5, -6e5, -7e5, -8e5, -9e5, -10e5, -1, false);


// Test negative values
let d4 = new Temporal.Duration(-1, -2, -3, -4, -5, -6, -7, -8, -9, -10);
assertDuration(d4.negated(), 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1, false);

let d5 = new Temporal.Duration(
    -1e5, -2e5, -3e5, -4e5, -5e5, -6e5, -7e5, -8e5, -9e5, -10e5);
assertDuration(d5.negated(),
    1e5, 2e5, 3e5, 4e5, 5e5, 6e5, 7e5, 8e5, 9e5, 10e5, 1, false);
