// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_non_literal_value() {
  function F() {}
  var val = 10;
  F.prototype.a = val; // Non-literal value, should not be optimized
  F.prototype.b = 2;
  return F;
}

function assert_test_non_literal_value(F) {
  assertEquals(10, F.prototype.a);
  assertEquals(2, F.prototype.b);
}

// prettier-ignore
function run(){
assert_test_non_literal_value(test_non_literal_value());
%CompileBaseline(test_non_literal_value);
assert_test_non_literal_value(test_non_literal_value());
%PrepareFunctionForOptimization(test_non_literal_value);
assert_test_non_literal_value(test_non_literal_value());
assert_test_non_literal_value(test_non_literal_value());
%OptimizeMaglevOnNextCall(test_non_literal_value);
assert_test_non_literal_value(test_non_literal_value());
assertOptimized(test_non_literal_value);
assertTrue(isMaglevved(test_non_literal_value));
assert_test_non_literal_value(test_non_literal_value());
%OptimizeFunctionOnNextCall(test_non_literal_value);
assert_test_non_literal_value(test_non_literal_value());
assertOptimized(test_non_literal_value);
assert_test_non_literal_value(test_non_literal_value());
}

run();
