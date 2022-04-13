// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

let inst1 = new Temporal.Instant(1234567890123456789n);
let inst2 = new Temporal.Instant(1234567890123456000n);
let inst3 = new Temporal.Instant(1234567890123456000n);

assertEquals(inst1.equals(inst2), false);
assertEquals(inst2.equals(inst3), true);

let badInst = {equals: inst1.equals};
assertThrows(() => badInst.equals(inst1), TypeError);

// TODO Test Temporal.compare with Temporal.ZonedDateTime object
// TODO Test Temporal.compare with TemporalInstantString
