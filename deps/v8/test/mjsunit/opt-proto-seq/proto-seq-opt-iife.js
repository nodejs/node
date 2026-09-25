// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_iife() {
  function test_func() {}

  test_func.prototype.smi = 1;
  test_func.prototype.iife = function () {
    return "test_function.prototype.iife";
  };

  return new test_func();
}

function assert_test_iife(test_func) {
  assertEquals(test_func.iife(), "test_function.prototype.iife");
  assertEquals(test_func.smi, 1);
}

// prettier-ignore
function run(){
assert_test_iife(test_iife());
%CompileBaseline(test_iife);
assert_test_iife(test_iife());
%PrepareFunctionForOptimization(test_iife);
assert_test_iife(test_iife());
assert_test_iife(test_iife());
%OptimizeMaglevOnNextCall(test_iife);
assert_test_iife(test_iife());
assertOptimized(test_iife);
assertTrue(isMaglevved(test_iife));
assert_test_iife(test_iife());
%OptimizeFunctionOnNextCall(test_iife);
assert_test_iife(test_iife());
assertOptimized(test_iife);
assert_test_iife(test_iife());
}

run();
