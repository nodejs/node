// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev

function round(x) {
  return Math.round(x);
}

%PrepareFunctionForOptimization(round);
assertEquals(0, round(0.1));
assertEquals(1, round(0.6));
assertEquals(-1, round(-0.6));
%OptimizeFunctionOnNextCall(round);

// IEEE-754 edge cases.
assertEquals(NaN, round(NaN));
assertEquals(Infinity, round(Infinity));
assertEquals(-Infinity, round(-Infinity));
assertTrue(Object.is(+0, round(+0)));
assertTrue(Object.is(-0, round(-0)));
assertTrue(Object.is(-0, round(-0.5)));
assertTrue(Object.is(-0, round(-0.1)));
assertEquals(-1, round(-0.5000000000000001));
assertTrue(Object.is(+0, round(0.49999999999999994)));
assertEquals(1, round(0.5));
assertEquals(-1, round(-1.5));
assertEquals(3, round(2.5));
assertEquals(-3, round(-3.5));
assertEquals(4503599627370496, round(4503599627370496));     // 2^52
assertEquals(-4503599627370496, round(-4503599627370496));   // -2^52
assertEquals(4503599627370495, round(4503599627370494.5));
assertEquals(-4503599627370495, round(-4503599627370495.5));
assertOptimized(round);
