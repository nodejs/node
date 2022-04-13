// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

let bigint1 = 1234567890123456789n;
let inst1 = new Temporal.Instant(bigint1);
let inst2 = Temporal.Instant.fromEpochNanoseconds(bigint1);
assertEquals(inst1, inst2);

let just_fit_neg_bigint = -8640000000000000000000n;
let just_fit_pos_bigint = 8640000000000000000000n;
let too_big_bigint = 8640000000000000000001n;
let too_small_bigint = -8640000000000000000001n;

assertThrows(() =>
    {let inst = Temporal.Instant.fromEpochNanoseconds(too_small_bigint)},
    RangeError);
assertThrows(() =>
    {let inst = Temporal.Instant.fromEpochNanoseconds(too_big_bigint)},
    RangeError);
assertEquals(just_fit_neg_bigint,
    (Temporal.Instant.fromEpochNanoseconds(
        just_fit_neg_bigint)).epochNanoseconds);
assertEquals(just_fit_pos_bigint,
    (Temporal.Instant.fromEpochNanoseconds(
        just_fit_pos_bigint)).epochNanoseconds);
