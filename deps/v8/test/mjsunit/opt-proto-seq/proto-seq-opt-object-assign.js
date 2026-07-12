// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --proto-assign-seq-lazy-func-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_object_assign(){
  function foo(){};
  foo.prototype.key_1 = function () {return "OK"};
  foo.prototype.key_2 = function () {};
  const returnedTarget = Object.assign({}, foo.prototype);
  return returnedTarget.key_1();
}

function assert_test_object_assign(result) {
  assertEquals(result, "OK");
}

// prettier-ignore
function run(){
assert_test_object_assign(test_object_assign());
%CompileBaseline(test_object_assign);
assert_test_object_assign(test_object_assign());
%PrepareFunctionForOptimization(test_object_assign);
assert_test_object_assign(test_object_assign());
assert_test_object_assign(test_object_assign());
%OptimizeMaglevOnNextCall(test_object_assign);
assert_test_object_assign(test_object_assign());
assertOptimized(test_object_assign);
assertTrue(isMaglevved(test_object_assign));
assert_test_object_assign(test_object_assign());
%OptimizeFunctionOnNextCall(test_object_assign);
assert_test_object_assign(test_object_assign());
assertOptimized(test_object_assign);
assert_test_object_assign(test_object_assign());
}

run();
