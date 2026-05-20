// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_has_setters() {
  function test_function() {}

  Object.defineProperty(test_function.prototype, "key", {
    set(x) {
      test_function.prototype = {};
    },
  });

  test_function.prototype.func = function () {
    return "test_function.prototype.func";
  };
  test_function.prototype.arrow_func = () => {
    return "test_function.prototype.arrow_func";
  };
  test_function.prototype.key = "key";
  test_function.prototype.smi = 1;
  test_function.prototype.str = "test_function.prototype.str";

  return new test_function();
}

function assert_test_has_setters(test_instance) {
  assertEquals(Object.keys(test_instance.__proto__).length, 2);
  assertEquals(test_instance.smi, 1);
  assertEquals(test_instance.str, "test_function.prototype.str");
}

// prettier-ignore
function run(){
assert_test_has_setters(test_has_setters());
%CompileBaseline(test_has_setters);
assert_test_has_setters(test_has_setters());
%PrepareFunctionForOptimization(test_has_setters);
assert_test_has_setters(test_has_setters());
assert_test_has_setters(test_has_setters());
%OptimizeMaglevOnNextCall(test_has_setters);
assert_test_has_setters(test_has_setters());
assertOptimized(test_has_setters);
assertTrue(isMaglevved(test_has_setters));
assert_test_has_setters(test_has_setters());
%OptimizeFunctionOnNextCall(test_has_setters);
assert_test_has_setters(test_has_setters());
assertOptimized(test_has_setters);
assert_test_has_setters(test_has_setters());
}

run();
