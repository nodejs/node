// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --proto-assign-seq-lazy-func-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_getOwnPropertyDescriptor(){
  function foo(){};
  foo.prototype.key_1 = function () {};
  foo.prototype.key_2 = function () {};
  var desc = Object.getOwnPropertyDescriptor(foo.prototype, "key_1");
  return desc.value;
}

function assert_test_getOwnPropertyDescriptor(result) {
  assertEquals(typeof result, "function");
}

// prettier-ignore
function run(){
assert_test_getOwnPropertyDescriptor(test_getOwnPropertyDescriptor());
%CompileBaseline(test_getOwnPropertyDescriptor);
assert_test_getOwnPropertyDescriptor(test_getOwnPropertyDescriptor());
%PrepareFunctionForOptimization(test_getOwnPropertyDescriptor);
assert_test_getOwnPropertyDescriptor(test_getOwnPropertyDescriptor());
assert_test_getOwnPropertyDescriptor(test_getOwnPropertyDescriptor());
%OptimizeMaglevOnNextCall(test_getOwnPropertyDescriptor);
assert_test_getOwnPropertyDescriptor(test_getOwnPropertyDescriptor());
assertOptimized(test_getOwnPropertyDescriptor);
assertTrue(isMaglevved(test_getOwnPropertyDescriptor));
assert_test_getOwnPropertyDescriptor(test_getOwnPropertyDescriptor());
%OptimizeFunctionOnNextCall(test_getOwnPropertyDescriptor);
assert_test_getOwnPropertyDescriptor(test_getOwnPropertyDescriptor());
assertOptimized(test_getOwnPropertyDescriptor);
assert_test_getOwnPropertyDescriptor(test_getOwnPropertyDescriptor());
}

run();
