// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_arrow_function() {
  var test_arrow_func = () => {};

  Object.defineProperty(test_arrow_func, "prototype", {
    value: {},
  });

  test_arrow_func.prototype.func = function () {
    return "test_function.prototype.func";
  };
  test_arrow_func.prototype.arrow_func = () => {
    return "test_function.prototype.arrow_func";
  };
  test_arrow_func.prototype.smi = 1;
  test_arrow_func.prototype.str = "test_function.prototype.str";
  return test_arrow_func;
}

function assert_test_arrow_function(test_arrow_func) {
  assertEquals(
    test_arrow_func.prototype.func(),
    "test_function.prototype.func",
  );
  assertEquals(
    test_arrow_func.prototype.arrow_func(),
    "test_function.prototype.arrow_func",
  );
  assertEquals(test_arrow_func.prototype.smi, 1);
  assertEquals(test_arrow_func.prototype.str, "test_function.prototype.str");
}

// prettier-ignore
function run(){
assert_test_arrow_function(test_arrow_function());
%CompileBaseline(test_arrow_function);
assert_test_arrow_function(test_arrow_function());
%PrepareFunctionForOptimization(test_arrow_function);
assert_test_arrow_function(test_arrow_function());
assert_test_arrow_function(test_arrow_function());
%OptimizeMaglevOnNextCall(test_arrow_function);
assert_test_arrow_function(test_arrow_function());
assertOptimized(test_arrow_function);
assertTrue(isMaglevved(test_arrow_function));
assert_test_arrow_function(test_arrow_function());
%OptimizeFunctionOnNextCall(test_arrow_function);
assert_test_arrow_function(test_arrow_function());
assertOptimized(test_arrow_function);
assert_test_arrow_function(test_arrow_function());
}

run();
