// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_assign_key_multiple_times() {
  function test_function() {}

  test_function.prototype.smi = function () {
    return "test_function.prototype.func";
  };
  test_function.prototype.smi = () => {
    return "test_function.prototype.arrow_func";
  };
  test_function.prototype.smi = 1;

  return new test_function();
}

function assert_test_assign_key_multiple_times(x) {
  assertEquals(x.smi, 1);
}

// prettier-ignore
function run(){
assert_test_assign_key_multiple_times(test_assign_key_multiple_times());
%CompileBaseline(test_assign_key_multiple_times);
assert_test_assign_key_multiple_times(test_assign_key_multiple_times());
%PrepareFunctionForOptimization(test_assign_key_multiple_times);
assert_test_assign_key_multiple_times(test_assign_key_multiple_times());
assert_test_assign_key_multiple_times(test_assign_key_multiple_times());
%OptimizeMaglevOnNextCall(test_assign_key_multiple_times);
assert_test_assign_key_multiple_times(test_assign_key_multiple_times());
assertOptimized(test_assign_key_multiple_times);
assertTrue(isMaglevved(test_assign_key_multiple_times));
assert_test_assign_key_multiple_times(test_assign_key_multiple_times());
%OptimizeFunctionOnNextCall(test_assign_key_multiple_times);
assert_test_assign_key_multiple_times(test_assign_key_multiple_times());
assertOptimized(test_assign_key_multiple_times);
assert_test_assign_key_multiple_times(test_assign_key_multiple_times());
}

run();
