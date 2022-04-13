// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test Infinity, -Infinity, NaN won't crash with any notation in formatToParts.

let validNotations = [
    "standard",
    "compact",
    "engineering",
    "scientific",
];

let tests = [
    123,
    Infinity,
    -Infinity,
    NaN
];

for (const notation of validNotations) {
  let nf = new Intl.NumberFormat("en", {notation});
  for (const test of tests) {
    assertDoesNotThrow(() => nf.format(test));
    assertDoesNotThrow(() => nf.formatToParts(test));
  }
}
