// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function min1(a) {
  return Math.min(a);
}
%PrepareFunctionForOptimization(min1);

min1(0.5, 1.5);

%OptimizeMaglevOnNextCall(min1);
assertEquals(6.1, min1(6.1));

// Passing a non-number will deopt.
assertEquals(1.5, min1({valueOf: () => 1.5}));

assertFalse(isMaglevved(min1));

%OptimizeMaglevOnNextCall(min1);
assertEquals(6.1, min1(6.1));

// No we no longer speculate on the argument type being float64, so we also
// don't deopt.
assertEquals(1.5, min1({valueOf: () => 1.5}));
assertTrue(isMaglevved(min1));
