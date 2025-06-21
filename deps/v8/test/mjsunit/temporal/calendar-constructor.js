// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// https://tc39.es/proposal-temporal/#sec-temporal.calendar
// 1. If NewTarget is undefined, then
// a. Throw a TypeError exception.
assertThrows(() => Temporal.Calendar("iso8601"), TypeError);

assertThrows(() => new Temporal.Calendar(), RangeError);

assertEquals("iso8601", (new Temporal.Calendar("IsO8601")).id)

assertEquals("iso8601", (new Temporal.Calendar("iso8601")).id)
