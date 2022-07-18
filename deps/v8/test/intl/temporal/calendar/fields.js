// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-temporal

assertEquals(["year"],
    (new Temporal.Calendar("iso8601")).fields(["year"]));

assertEquals(["month", "year"],
    (new Temporal.Calendar("iso8601")).fields(["month", "year"]));

assertEquals(["year", "era", "eraYear"],
    (new Temporal.Calendar("japanese")).fields(["year"]));

assertEquals(["month", "year", "era", "eraYear"],
    (new Temporal.Calendar("japanese")).fields(["month", "year"]));

assertEquals(["year", "era", "eraYear"],
    (new Temporal.Calendar("roc")).fields(["year"]));

assertThrows(
    () => (new Temporal.Calendar("japanese")).fields(["hello", "world"]),
    RangeError);
