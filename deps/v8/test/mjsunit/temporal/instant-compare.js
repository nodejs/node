// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

let inst1 = new Temporal.Instant(1234567890123456789n);
let inst2 = new Temporal.Instant(1234567890123456000n);
let inst3 = new Temporal.Instant(1234567890123456000n);

assertEquals(Temporal.Instant.compare(inst2, inst3), 0);
assertEquals(Temporal.Instant.compare(inst1, inst2), 1);
assertEquals(Temporal.Instant.compare(inst3, inst1), -1);

assertThrows(() => Temporal.Instant.compare(inst1, "invalid iso8601 string"),
    RangeError);
assertThrows(() => Temporal.Instant.compare("invalid iso8601 string", inst1),
    RangeError);


// TODO Test Temporal.compare with Temporal.ZonedDateTime object
// TODO Test Temporal.compare with TemporalInstantString
