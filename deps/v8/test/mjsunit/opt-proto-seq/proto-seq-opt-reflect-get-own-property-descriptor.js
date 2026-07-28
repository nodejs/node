// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --proto-assign-seq-lazy-func-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_reflect_getOwnPropertyDescriptor(){
  function foo(){};
  foo.prototype.key_1 = function () {};
  foo.prototype.key_2 = function () {};
  var desc = Reflect.getOwnPropertyDescriptor(foo.prototype, "key_1");
  return desc.value;
}

function assert_test_reflect_getOwnPropertyDescriptor(result) {
  assertEquals(typeof result, "function");
}

// prettier-ignore
function run(){
assert_test_reflect_getOwnPropertyDescriptor(test_reflect_getOwnPropertyDescriptor());
%CompileBaseline(test_reflect_getOwnPropertyDescriptor);
assert_test_reflect_getOwnPropertyDescriptor(test_reflect_getOwnPropertyDescriptor());
%PrepareFunctionForOptimization(test_reflect_getOwnPropertyDescriptor);
assert_test_reflect_getOwnPropertyDescriptor(test_reflect_getOwnPropertyDescriptor());
assert_test_reflect_getOwnPropertyDescriptor(test_reflect_getOwnPropertyDescriptor());
%OptimizeMaglevOnNextCall(test_reflect_getOwnPropertyDescriptor);
assert_test_reflect_getOwnPropertyDescriptor(test_reflect_getOwnPropertyDescriptor());
assertOptimized(test_reflect_getOwnPropertyDescriptor);
assertTrue(isMaglevved(test_reflect_getOwnPropertyDescriptor));
assert_test_reflect_getOwnPropertyDescriptor(test_reflect_getOwnPropertyDescriptor());
%OptimizeFunctionOnNextCall(test_reflect_getOwnPropertyDescriptor);
assert_test_reflect_getOwnPropertyDescriptor(test_reflect_getOwnPropertyDescriptor());
assertOptimized(test_reflect_getOwnPropertyDescriptor);
assert_test_reflect_getOwnPropertyDescriptor(test_reflect_getOwnPropertyDescriptor());
}

run();
