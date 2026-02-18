// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_constructor_assignment() {
  function F() {}
  F.prototype.a = 1;
  F.prototype.constructor = function G() {};
  F.prototype.b = 2;
  return F;
}

function assert_test_constructor_assignment(F) {
  assertEquals(1, F.prototype.a);
  assertEquals("G", F.prototype.constructor.name);
  assertEquals(2, F.prototype.b);
}

// prettier-ignore
function run(){
assert_test_constructor_assignment(test_constructor_assignment());
%CompileBaseline(test_constructor_assignment);
assert_test_constructor_assignment(test_constructor_assignment());
%PrepareFunctionForOptimization(test_constructor_assignment);
assert_test_constructor_assignment(test_constructor_assignment());
assert_test_constructor_assignment(test_constructor_assignment());
%OptimizeMaglevOnNextCall(test_constructor_assignment);
assert_test_constructor_assignment(test_constructor_assignment());
assertOptimized(test_constructor_assignment);
assertTrue(isMaglevved(test_constructor_assignment));
assert_test_constructor_assignment(test_constructor_assignment());
%OptimizeFunctionOnNextCall(test_constructor_assignment);
assert_test_constructor_assignment(test_constructor_assignment());
assertOptimized(test_constructor_assignment);
assert_test_constructor_assignment(test_constructor_assignment());
}

run();
