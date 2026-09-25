// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_computed_property() {
  function F() {}
  const name = "a";
  F.prototype[name] = 1; // Computed property, should not be optimized
  F.prototype.b = 2;
  return F;
}

function assert_test_computed_property(F) {
  assertEquals(1, F.prototype.a);
  assertEquals(2, F.prototype.b);
}

// prettier-ignore
function run(){
assert_test_computed_property(test_computed_property());
%CompileBaseline(test_computed_property);
assert_test_computed_property(test_computed_property());
%PrepareFunctionForOptimization(test_computed_property);
assert_test_computed_property(test_computed_property());
assert_test_computed_property(test_computed_property());
%OptimizeMaglevOnNextCall(test_computed_property);
assert_test_computed_property(test_computed_property());
assertOptimized(test_computed_property);
assertTrue(isMaglevved(test_computed_property));
assert_test_computed_property(test_computed_property());
%OptimizeFunctionOnNextCall(test_computed_property);
assert_test_computed_property(test_computed_property());
assertOptimized(test_computed_property);
assert_test_computed_property(test_computed_property());
}

run();
