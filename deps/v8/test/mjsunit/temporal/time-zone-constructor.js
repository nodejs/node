// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// 1. If NewTarget is undefined, then
// a. Throw a TypeError exception.
assertThrows(() => Temporal.TimeZone("UTC"), TypeError);

assertThrows(() => new Temporal.TimeZone(), RangeError);

assertEquals("UTC", (new Temporal.TimeZone("utc")).id)
