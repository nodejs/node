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

// Passing a non-number will deopt.
assertEquals(1.5, max2(0.5, {valueOf: () => 1.5}));

assertFalse(isMaglevved(max2));

%OptimizeMaglevOnNextCall(max2);
assertEquals(6.2, max2(6.1, 6.2));

// No we no longer speculate on the argument type being float64, so we also
// don't deopt.
assertEquals(1.5, max2(0.5, {valueOf: () => 1.5}));
assertTrue(isMaglevved(max2));
