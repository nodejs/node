// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

let bigint_nano = 1234567890123456789000n;
let bigint_micro = 1234567890123456789n;
let inst1 = new Temporal.Instant(bigint_nano);
let inst2 = Temporal.Instant.fromEpochMicroseconds(bigint_micro);
assertEquals(inst1, inst2);

let just_fit_neg_bigint = -8640000000000000000n;
let just_fit_pos_bigint = 8640000000000000000n;
let too_big_bigint = 8640000000000000001n;
let too_small_bigint = -8640000000000000001n;

assertThrows(() =>
    {let inst = Temporal.Instant.fromEpochMicroseconds(too_small_bigint)},
    RangeError);
assertThrows(() =>
    {let inst = Temporal.Instant.fromEpochMicroseconds(too_big_bigint)},
    RangeError);
assertEquals(just_fit_neg_bigint,
    (Temporal.Instant.fromEpochMicroseconds(
        just_fit_neg_bigint)).epochMicroseconds);
assertEquals(just_fit_pos_bigint,
    (Temporal.Instant.fromEpochMicroseconds(
        just_fit_pos_bigint)).epochMicroseconds);
