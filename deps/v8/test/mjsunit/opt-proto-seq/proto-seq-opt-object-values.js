// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --proto-assign-seq-lazy-func-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_object_values() {
  function foo(){};
  foo.prototype.key_1 = function () {return "ok"};
  foo.prototype.key_2 = function () {};
  return Object.values(foo.prototype);
}

function assert_test_object_values(result) {
  assertEquals(result.length, 2);
}

// prettier-ignore
function run(){
assert_test_object_values(test_object_values());
%CompileBaseline(test_object_values);
assert_test_object_values(test_object_values());
%PrepareFunctionForOptimization(test_object_values);
assert_test_object_values(test_object_values());
assert_test_object_values(test_object_values());
%OptimizeMaglevOnNextCall(test_object_values);
assert_test_object_values(test_object_values());
assertOptimized(test_object_values);
assertTrue(isMaglevved(test_object_values));
assert_test_object_values(test_object_values());
%OptimizeFunctionOnNextCall(test_object_values);
assert_test_object_values(test_object_values());
assertOptimized(test_object_values);
assert_test_object_values(test_object_values());
}

run();
