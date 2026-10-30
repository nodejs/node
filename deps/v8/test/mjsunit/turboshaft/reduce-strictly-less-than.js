// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// Signed 32-bit integer comparison: (x | 0)
function test_signed_lt(x) {
  x = x | 0;
  let y = (x + 1) | 0;
  y = (y - 1) | 0; // Simplified to x
  return x < y;    // Lowers to Turboshaft kSignedLessThan (Word32)
}

function test_signed_le(x) {
  x = x | 0;
  let y = (x + 1) | 0;
  y = (y - 1) | 0; // Simplified to x
  return x <= y;   // Lowers to Turboshaft kSignedLessThanOrEqual (Word32)
}

%PrepareFunctionForOptimization(test_signed_lt);
%PrepareFunctionForOptimization(test_signed_le);

assertFalse(test_signed_lt(42));
assertTrue(test_signed_le(42));

%OptimizeFunctionOnNextCall(test_signed_lt);
%OptimizeFunctionOnNextCall(test_signed_le);

assertFalse(test_signed_lt(42));
assertTrue(test_signed_le(42));

assertOptimized(test_signed_lt);
assertOptimized(test_signed_le);
