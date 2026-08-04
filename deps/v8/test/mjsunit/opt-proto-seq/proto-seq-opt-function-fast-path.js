// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_function_fast_path() {
  function test_function() {}

  test_function.prototype.func = function () {
    return "test_function.prototype.func";
  };
  test_function.prototype.arrow_func = () => {
    return "test_function.prototype.arrow_func";
  };
  test_function.prototype.smi = 1;
  test_function.prototype.str = "test_function.prototype.str";

  return new test_function();
}

function assert_test_function_fast_path(test_instance) {
  assertEquals(test_instance.func(), "test_function.prototype.func");
  assertEquals(
    test_instance.arrow_func(),
    "test_function.prototype.arrow_func",
  );
  assertEquals(test_instance.smi, 1);
  assertEquals(test_instance.str, "test_function.prototype.str");
}

// prettier-ignore
function run(){
assert_test_function_fast_path(test_function_fast_path());
%CompileBaseline(test_function_fast_path);
assert_test_function_fast_path(test_function_fast_path());
%PrepareFunctionForOptimization(test_function_fast_path);
assert_test_function_fast_path(test_function_fast_path());
assert_test_function_fast_path(test_function_fast_path());
%OptimizeMaglevOnNextCall(test_function_fast_path);
assert_test_function_fast_path(test_function_fast_path());
assertOptimized(test_function_fast_path);
assertTrue(isMaglevved(test_function_fast_path));
assert_test_function_fast_path(test_function_fast_path());
%OptimizeFunctionOnNextCall(test_function_fast_path);
assert_test_function_fast_path(test_function_fast_path());
assertOptimized(test_function_fast_path);
assert_test_function_fast_path(test_function_fast_path());
}

run();
