// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.fields
let cal = new Temporal.Calendar("iso8601")

assertEquals("iso8601", cal.id)

const fields = {
  *[Symbol.iterator]() {
      let i = 0;
      while (i++ < 1000) {
        yield "year";
      }
  }
}

let expected = Array.from(fields);
// For now, we only input it as array
let inpiut = expected;
assertArrayEquals(expected, cal.fields(expected));
