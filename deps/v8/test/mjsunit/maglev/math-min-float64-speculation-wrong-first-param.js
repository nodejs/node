// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function min2(a, b) {
  return Math.min(a, b);
}
%PrepareFunctionForOptimization(min2);

min2(0.5, 1.5);

%OptimizeMaglevOnNextCall(min2);
assertEquals(6.1, min2(6.1, 6.2));

// Passing a non-number will deopt.
assertEquals(0.5, min2({valueOf: () => 1.5}, 0.5));

assertFalse(isMaglevved(min2));

%OptimizeMaglevOnNextCall(min2);
assertEquals(6.1, min2(6.1, 6.2));

// No we no longer speculate on the argument type being float64, so we also
// don't deopt.
assertEquals(0.5, min2({valueOf: () => 1.5}, 0.5));
assertTrue(isMaglevved(min2));
