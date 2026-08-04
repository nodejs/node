// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --proto-assign-seq-lazy-func-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_getOwnPropertyDescriptors(){
  function foo(){};
  foo.prototype.key_1 = function () {};
  foo.prototype.key_2 = function () {};
  var desc = Object.getOwnPropertyDescriptors(foo.prototype);
  return desc.key_1.value;
}

function assert_test_getOwnPropertyDescriptors(result) {
  assertEquals(typeof result, "function");
}

// prettier-ignore
function run(){
assert_test_getOwnPropertyDescriptors(test_getOwnPropertyDescriptors());
%CompileBaseline(test_getOwnPropertyDescriptors);
assert_test_getOwnPropertyDescriptors(test_getOwnPropertyDescriptors());
%PrepareFunctionForOptimization(test_getOwnPropertyDescriptors);
assert_test_getOwnPropertyDescriptors(test_getOwnPropertyDescriptors());
assert_test_getOwnPropertyDescriptors(test_getOwnPropertyDescriptors());
%OptimizeMaglevOnNextCall(test_getOwnPropertyDescriptors);
assert_test_getOwnPropertyDescriptors(test_getOwnPropertyDescriptors());
assertOptimized(test_getOwnPropertyDescriptors);
assertTrue(isMaglevved(test_getOwnPropertyDescriptors));
assert_test_getOwnPropertyDescriptors(test_getOwnPropertyDescriptors());
%OptimizeFunctionOnNextCall(test_getOwnPropertyDescriptors);
assert_test_getOwnPropertyDescriptors(test_getOwnPropertyDescriptors());
assertOptimized(test_getOwnPropertyDescriptors);
assert_test_getOwnPropertyDescriptors(test_getOwnPropertyDescriptors());
}

run();
