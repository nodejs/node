// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Case 1: Non-optimized (dynamic string)
function test_dynamic(str) {
  return str.length;
}
%PrepareFunctionForOptimization(test_dynamic);
assertEquals(3, test_dynamic("abc"));
assertEquals(5, test_dynamic("defgh"));
%OptimizeMaglevOnNextCall(test_dynamic);
assertEquals(3, test_dynamic("abc"));
assertEquals(5, test_dynamic("defgh"));

// Case 2: Optimized (should be folded)
function test_folded(b) {
  var x = 0;
  if (b) {
    x = 1;
  } else {
    x = 1;
  }
  var str = "def";
  if (x === 1) {
    str = "abc";
  } else {
    str = "defgh";
  }
  return str.length;
}
%PrepareFunctionForOptimization(test_folded);
test_folded(true);
test_folded(false);
%OptimizeMaglevOnNextCall(test_folded);
assertEquals(3, test_folded(true));
