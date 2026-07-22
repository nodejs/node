// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

let inst1 = new Temporal.Instant(1234567890123456789n);
assertEquals(1234567890123456789n , inst1.epochNanoseconds);
assertEquals(1234567890123456n , inst1.epochMicroseconds);
assertEquals(1234567890123 , inst1.epochMilliseconds);
assertEquals(1234567890 , inst1.epochSeconds);

let inst2 = new Temporal.Instant(-1234567890123456789n);
assertEquals(-1234567890123456789n , inst2.epochNanoseconds);
assertEquals(-1234567890123456n , inst2.epochMicroseconds);
assertEquals(-1234567890123 , inst2.epochMilliseconds);
assertEquals(-1234567890 , inst2.epochSeconds);

// 1. If NewTarget is undefined, then
// a. Throw a TypeError exception.
assertThrows(() => Temporal.Instant(1234567890123456789n), TypeError);

// 2. Let epochNanoseconds be ? ToBigInt(epochNanoseconds).
assertThrows(() => {let inst = new Temporal.Instant(undefined)},
    TypeError);
assertThrows(() => {let inst = new Temporal.Instant(null)}, TypeError);
assertEquals(1n, (new Temporal.Instant(true)).epochNanoseconds);
assertEquals(0n, (new Temporal.Instant(false)).epochNanoseconds);
assertThrows(() => {let inst = Temporal.Instant(12345)}, TypeError);
assertEquals(1234567890123456789n,
    (new Temporal.Instant("1234567890123456789")).epochNanoseconds);
assertThrows(() => {let inst = new Temporal.Instant(Symbol(12345n))},
    TypeError);

// 3. If ! IsValidEpochNanoseconds(epochNanoseconds) is false,
// throw a RangeError exception.
assertThrows(() => {let inst = new Temporal.Instant(8640000000000000000001n)},
    RangeError);
assertThrows(() => {let inst = new Temporal.Instant(-8640000000000000000001n)},
    RangeError);
assertEquals(8640000000000000000000n,
    (new Temporal.Instant(8640000000000000000000n)).epochNanoseconds);
assertEquals(-8640000000000000000000n,
    (new Temporal.Instant(-8640000000000000000000n)).epochNanoseconds);
