// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --proto-assign-seq-lazy-func-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_reflect_get(){
  function foo(){};
  foo.prototype.key_1 = function () {return "OK"};
  foo.prototype.key_2 = function () {};
  return Reflect.get(foo.prototype, "key_1")();
}

function assert_test_reflect_get(result) {
  assertEquals(result, "OK");
}

// prettier-ignore
function run(){
assert_test_reflect_get(test_reflect_get());
%CompileBaseline(test_reflect_get);
assert_test_reflect_get(test_reflect_get());
%PrepareFunctionForOptimization(test_reflect_get);
assert_test_reflect_get(test_reflect_get());
assert_test_reflect_get(test_reflect_get());
%OptimizeMaglevOnNextCall(test_reflect_get);
assert_test_reflect_get(test_reflect_get());
assertOptimized(test_reflect_get);
assertTrue(isMaglevved(test_reflect_get));
assert_test_reflect_get(test_reflect_get());
%OptimizeFunctionOnNextCall(test_reflect_get);
assert_test_reflect_get(test_reflect_get());
assertOptimized(test_reflect_get);
assert_test_reflect_get(test_reflect_get());
}

run();
