// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Except when the notation is "compact", the resolvedOptions().compactDisplay
// should be undefined.
//
// Test default
let nf = new Intl.NumberFormat();
assertEquals(undefined, nf.resolvedOptions().compactDisplay);

nf = new Intl.NumberFormat("en");
assertEquals(undefined, nf.resolvedOptions().compactDisplay);

const testData = [
    ["scientific"],
    ["engineering"],
    ["standard"],
];

for (const [notation] of testData) {
  nf = new Intl.NumberFormat("en", {notation});
  assertEquals(undefined, nf.resolvedOptions().compactDisplay);
  for (const compactDisplay of ["short", "long"]) {
    nf = new Intl.NumberFormat("en", {compactDisplay, notation});
    assertEquals(undefined, nf.resolvedOptions().compactDisplay);
  }
}
