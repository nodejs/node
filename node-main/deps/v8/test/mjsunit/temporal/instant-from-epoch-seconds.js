// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

let bigint_nano = 7890123456789000000000n;
let sec = 7890123456789;
let bigint_sec = BigInt(sec);
let inst1 = new Temporal.Instant(bigint_nano);
assertThrows(() =>
    Temporal.Instant.fromEpochSeconds(bigint_sec),
    TypeError)
let inst2 = Temporal.Instant.fromEpochSeconds(sec);
assertEquals(inst1, inst2);

let just_fit_neg = -8640000000000;
let just_fit_pos = 8640000000000;
let too_big = 8640000000001;
let too_small = -8640000000001;

assertThrows(() =>
    Temporal.Instant.fromEpochSeconds(too_small),
    RangeError)
assertThrows(() =>
    Temporal.Instant.fromEpochSeconds(too_big),
    RangeError)
assertEquals(just_fit_neg,
    (Temporal.Instant.fromEpochSeconds(just_fit_neg)).epochSeconds);
assertEquals(just_fit_pos,
    (Temporal.Instant.fromEpochSeconds(just_fit_pos)).epochSeconds);
