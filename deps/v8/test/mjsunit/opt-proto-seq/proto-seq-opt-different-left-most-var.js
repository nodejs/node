// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_different_left_most_var() {
  // Note: This code should not generate a SetPrototypeProperties Instruction
  function foo() {}
  function bar() {}

  foo.prototype.k1 = 1;
  bar.prototype.k1 = 2;
  foo.prototype.k2 = 3;
  bar.prototype.k2 = 4;

  return [foo, bar];
}

function assert_test_different_left_most_var(arr) {
  assertEquals(arr[0].prototype.k1, 1);
  assertEquals(arr[1].prototype.k1, 2);
  assertEquals(arr[0].prototype.k2, 3);
  assertEquals(arr[1].prototype.k2, 4);
}

// prettier-ignore
function run(){
assert_test_different_left_most_var(test_different_left_most_var());
%CompileBaseline(test_different_left_most_var);
assert_test_different_left_most_var(test_different_left_most_var());
%PrepareFunctionForOptimization(test_different_left_most_var);
assert_test_different_left_most_var(test_different_left_most_var());
assert_test_different_left_most_var(test_different_left_most_var());
%OptimizeMaglevOnNextCall(test_different_left_most_var);
assert_test_different_left_most_var(test_different_left_most_var());
assertOptimized(test_different_left_most_var);
assertTrue(isMaglevved(test_different_left_most_var));
assert_test_different_left_most_var(test_different_left_most_var());
%OptimizeFunctionOnNextCall(test_different_left_most_var);
assert_test_different_left_most_var(test_different_left_most_var());
assertOptimized(test_different_left_most_var);
assert_test_different_left_most_var(test_different_left_most_var());
}

run();
