// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal


assertEquals("1970-01-01T00:00:00Z",
    (Temporal.Instant.fromEpochSeconds(0n)).toJSON());

let days_in_sec = 24n * 60n * 60n;
assertEquals("1970-12-31T23:59:59Z",
    Temporal.Instant.fromEpochSeconds((365n * days_in_sec) - 1n).toJSON());
assertEquals("1971-01-01T00:00:00Z",
    Temporal.Instant.fromEpochSeconds((365n * days_in_sec)).toJSON());

assertEquals("1971-12-31T23:59:59Z",
    Temporal.Instant.fromEpochSeconds((2n *365n * days_in_sec - 1n)).toJSON());
assertEquals("1972-01-01T00:00:00Z",
    Temporal.Instant.fromEpochSeconds((2n *365n * days_in_sec)).toJSON());

// 1972 is a leap year
assertEquals("1972-02-28T00:00:00Z",
    Temporal.Instant.fromEpochSeconds(((2n *365n + 58n) * days_in_sec)).toJSON());
assertEquals("1972-02-29T00:00:00Z",
    Temporal.Instant.fromEpochSeconds(((2n *365n + 59n) * days_in_sec)).toJSON());

assertEquals("1985-01-01T00:00:00Z",
    Temporal.Instant.fromEpochSeconds(((15n *365n + 4n) * days_in_sec)).toJSON());

// Test with Date

const year_in_sec = 24*60*60*365;
const number_of_random_test = 500;
for (i = 0; i < number_of_random_test ; i++) {
  // bertween -5000 years and +5000 years
  let ms = Math.floor(Math.random() * year_in_sec * 1000 * 10000) - year_in_sec * 1000 * 5000;
  // Temporal auto precision will remove trailing zeros in milliseconds so we only
  // compare the first 19 char- to second.
  let d = new Date(ms)
  let bigd = BigInt(d)
  dateout = d.toJSON().substr(0,19);
  temporalout = Temporal.Instant.fromEpochMilliseconds(bigd).toJSON().substr(0, 19);
  if (dateout[0] != '0') {
    assertEquals(dateout, temporalout, ms);
  }
}
