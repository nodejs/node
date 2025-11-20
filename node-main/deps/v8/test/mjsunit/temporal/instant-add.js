// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

let i1 = new Temporal.Instant(50000n);
assertEquals(3052001n,
    i1.add(new Temporal.Duration(0,0,0,0,0,0,0,3,2,1)).epochNanoseconds);

assertEquals(BigInt(4 * 1e9) + 3052001n,
    i1.add(new Temporal.Duration(0,0,0,0,0,0,4,3,2,1)).epochNanoseconds);

assertEquals(BigInt(5 * 60 + 4) * 1000000000n + 3052001n,
    i1.add(new Temporal.Duration(0,0,0,0,0,5,4,3,2,1)).epochNanoseconds);

assertEquals(BigInt(6 * 3600 + 5 * 60 + 4) * 1000000000n + 3052001n,
    i1.add(new Temporal.Duration(0,0,0,0,6,5,4,3,2,1)).epochNanoseconds);

assertEquals(-2952001n,
    i1.add(new Temporal.Duration(0,0,0,0,0,0,0,-3,-2,-1)).epochNanoseconds);

assertEquals(BigInt(-4 * 1e9) - 2952001n,
    i1.add(new Temporal.Duration(0,0,0,0,0,0,-4,-3,-2,-1)).epochNanoseconds);

assertEquals(BigInt(5 * 60 + 4) * -1000000000n - 2952001n,
    i1.add(new Temporal.Duration(0,0,0,0,0,-5,-4,-3,-2,-1)).epochNanoseconds);

assertEquals(BigInt(6 * 3600 + 5 * 60 + 4) * -1000000000n - 2952001n,
    i1.add(new Temporal.Duration(0,0,0,0,-6,-5,-4,-3,-2,-1)).epochNanoseconds);


// Test  RequireInternalSlot Throw TypeError
let badInstant = { add: i1.add };
assertThrows(() => badInstant.add(new Temporal.Duration(0, 0, 0, 0, 5)), TypeError);

// Test ToLimitedTemporalDuration Throw RangeError
assertThrows(() => i1.add(new Temporal.Duration(1)), RangeError);
assertThrows(() => i1.add(new Temporal.Duration(0, 2)), RangeError);
assertThrows(() => i1.add(new Temporal.Duration(0, 0, 3)), RangeError);
assertThrows(() => i1.add(new Temporal.Duration(0, 0, 0, 4)), RangeError);
assertThrows(() => i1.add(new Temporal.Duration(-1)), RangeError);
assertThrows(() => i1.add(new Temporal.Duration(0, -2)), RangeError);
assertThrows(() => i1.add(new Temporal.Duration(0, 0, -3)), RangeError);
assertThrows(() => i1.add(new Temporal.Duration(0, 0, 0, -4)), RangeError);

// Test AddInstant Throw RangeError
let i2 = new Temporal.Instant(86400n * 99999999999999999n);
assertThrows(() => i2.add(new Temporal.Duration(0, 0, 0, 0, 999999999)), RangeError);
