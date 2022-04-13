// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test default.
let nf = new Intl.NumberFormat();
assertEquals("auto", nf.resolvedOptions().signDisplay);

nf = new Intl.NumberFormat("en");
assertEquals("auto", nf.resolvedOptions().signDisplay);

const testData = [
    ["auto",        "-123",  "-0",  "0",  "123"],
    ["always",      "-123",  "-0", "+0", "+123"],
    ["never",       "123",   "0",  "0",  "123"],
    ["exceptZero",  "-123",  "0",  "0",  "+123"],
];

for (const [signDisplay, neg, negZero, zero, pos] of testData) {
  nf = new Intl.NumberFormat("en", {signDisplay});
  assertEquals(signDisplay, nf.resolvedOptions().signDisplay);
  assertEquals(neg, nf.format(-123));
  assertEquals(negZero, nf.format(-0));
  assertEquals(zero, nf.format(0));
  assertEquals(pos, nf.format(123));
}
