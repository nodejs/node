// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.fields
let cal = new Temporal.Calendar("iso8601")

assertEquals("iso8601", cal.id)

let i = 1;
const repeated = {
  *[Symbol.iterator]() {
      yield "year";
      i++;
      yield "year";
      i++;
  }
}

assertThrows(() => cal.fields(repeated), RangeError);
assertEquals(2, i);
let repeatedArray = Array.from(repeated);
assertThrows(() => cal.fields(repeatedArray), RangeError);

const week = {
  *[Symbol.iterator]() {
      yield "week";
  }
}

assertThrows(() => cal.fields(week), RangeError);
assertThrows(() => cal.fields(['week']), RangeError);
assertThrows(() => cal.fields(new Set(['week'])), RangeError);

const allValid = {
  *[Symbol.iterator]() {
      yield "nanosecond";
      yield "microsecond";
      yield "millisecond";
      yield "second";
      yield "minute";
      yield "hour";
      yield "day";
      yield "monthCode";
      yield "month";
      yield "year";
  }
}

let allValidArray = Array.from(allValid);
let allValidSet = new Set(allValid);
assertArrayEquals(allValidArray, cal.fields(allValid));
assertArrayEquals(allValidArray, cal.fields(allValidArray));
assertArrayEquals(allValidArray, cal.fields(allValidSet));

// cannot just return the same array
assertTrue(allValidArray != cal.fields(allValidArray));
