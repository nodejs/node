// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// https://tc39.es/proposal-temporal/#sec-temporal.calendar
// 1. If NewTarget is undefined, then
// a. Throw a TypeError exception.
assertThrows(() => Temporal.Calendar("iso8601"), TypeError,
    "Constructor Temporal.Calendar requires 'new'");

assertThrows(() => new Temporal.Calendar(), RangeError,
    "Invalid calendar specified: undefined");

// Wrong case
assertThrows(() => new Temporal.Calendar("ISO8601"), RangeError,
    "Invalid calendar specified: ISO8601");

assertEquals("iso8601", (new Temporal.Calendar("iso8601")).id)
