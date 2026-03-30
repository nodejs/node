// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function min2(a, b) {
  return Math.min(a, b);
}
%PrepareFunctionForOptimization(min2);

min2(0.5, 1.5);

%OptimizeFunctionOnNextCall(min2);

assertEquals(6.1, min2(6.1, 6.2));
assertEquals(6.1, min2(6.2, 6.1));

assertEquals(-6.2, min2(-6.1, -6.2));
assertEquals(-6.2, min2(-6.2, -6.1));

assertTrue(Object.is(-0, min2(-0.0, 0.0)));
assertTrue(Object.is(-0, min2(0.0, -0.0)));
assertFalse(Object.is(-0, min2(0.0, 0.0)));
assertTrue(Object.is(-0, min2(-0.0, -0.0)));

assertTrue(isNaN(min2(5.2, NaN)));
assertTrue(isNaN(min2(NaN, 5.3)));

assertSame(Infinity, min2(Infinity, Infinity));
assertSame(-Infinity, min2(Infinity, -Infinity));
assertSame(-Infinity, min2(-Infinity, Infinity));
assertSame(-Infinity, min2(-Infinity, -Infinity));

// No deopts.
assertOptimized(min2);
