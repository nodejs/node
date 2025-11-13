// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function max2(a, b) {
  return Math.max(a, b);
}
%PrepareFunctionForOptimization(max2);

max2(0.5, 1.5);

%OptimizeMaglevOnNextCall(max2);

assertEquals(6.2, max2(6.1, 6.2));
assertEquals(6.2, max2(6.2, 6.1));

assertEquals(-6.1, max2(-6.1, -6.2));
assertEquals(-6.1, max2(-6.2, -6.1));

assertFalse(Object.is(-0, max2(-0.0, 0.0)));
assertFalse(Object.is(-0, max2(0.0, -0.0)));
assertFalse(Object.is(-0, max2(0.0, 0.0)));
assertTrue(Object.is(-0, max2(-0.0, -0.0)));

assertTrue(isNaN(max2(5.2, NaN)));
assertTrue(isNaN(max2(NaN, 5.3)));

assertSame(Infinity, max2(Infinity, Infinity));
assertSame(Infinity, max2(Infinity, -Infinity));
assertSame(Infinity, max2(-Infinity, Infinity));
assertSame(-Infinity, max2(-Infinity, -Infinity));

// No deopts.
assertTrue(isMaglevved(max2));
