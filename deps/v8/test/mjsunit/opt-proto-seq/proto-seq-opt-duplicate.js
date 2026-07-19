// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_duplicate_properties() {
  function F() {}
  F.prototype.a = 1;
  F.prototype.b = 2;
  // Duplicate key breaks the sequence in the builder.
  // The first two assignments (a, b) might be optimized if count=2.
  // The third (a=3) will be a separate store.
  F.prototype.a = 3;
  return F;
}

function assert_test_duplicate_properties(F) {
  assertEquals(3, F.prototype.a);
  assertEquals(2, F.prototype.b);
}

// prettier-ignore
function run(){
assert_test_duplicate_properties(test_duplicate_properties());
%CompileBaseline(test_duplicate_properties);
assert_test_duplicate_properties(test_duplicate_properties());
%PrepareFunctionForOptimization(test_duplicate_properties);
assert_test_duplicate_properties(test_duplicate_properties());
assert_test_duplicate_properties(test_duplicate_properties());
%OptimizeMaglevOnNextCall(test_duplicate_properties);
assert_test_duplicate_properties(test_duplicate_properties());
assertOptimized(test_duplicate_properties);
assertTrue(isMaglevved(test_duplicate_properties));
assert_test_duplicate_properties(test_duplicate_properties());
%OptimizeFunctionOnNextCall(test_duplicate_properties);
assert_test_duplicate_properties(test_duplicate_properties());
assertOptimized(test_duplicate_properties);
assert_test_duplicate_properties(test_duplicate_properties());
}

run();
