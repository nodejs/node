// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function assertDoubleBits(hi, lo, x) {
  hi = hi | 0;
  lo = lo | 0;
  assertEquals(x, %_ConstructDouble(hi, lo));
  assertEquals(hi, %_DoubleHi(x));
  assertEquals(lo, %_DoubleLo(x));
  assertEquals(x, %_ConstructDouble(%_DoubleHi(x), %_DoubleLo(x)));
}


var tests = [0x7ff00000, 0x00000000, Infinity,
             0xfff00000, 0x00000000, -Infinity,
             0x80000000, 0x00000000, -0,
             0x400921fb, 0x54442d18, Math.PI,
             0xc00921fb, 0x54442d18, -Math.PI,
             0x4005bf0a, 0x8b145769, Math.E,
             0xc005bf0a, 0x8b145769, -Math.E,
             0xbfe80000, 0x00000000, -0.75];


for (var i = 0; i < tests.length; i += 3) {
  assertDoubleBits(tests[i], tests[i + 1], tests[i + 2]);
}

%OptimizeFunctionOnNextCall(assertDoubleBits);

for (var i = 0; i < tests.length; i += 3) {
  assertDoubleBits(tests[i], tests[i + 1], tests[i + 2]);
  assertOptimized(assertDoubleBits);
}
