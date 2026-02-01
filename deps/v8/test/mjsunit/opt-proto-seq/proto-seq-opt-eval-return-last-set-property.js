// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_eval_return_last_set_property() {
  return eval(`
    function Foo(){}
    Foo.prototype.k1 = 1;
    Foo.prototype.k2 = 2;
    Foo.prototype.k3 = 3;
  `);
}

function assert_test_eval_return_last_set_property(result) {
  assertEquals(3, result);
}

// prettier-ignore
function run(){
assert_test_eval_return_last_set_property(test_eval_return_last_set_property());
%CompileBaseline(test_eval_return_last_set_property);
assert_test_eval_return_last_set_property(test_eval_return_last_set_property());
%PrepareFunctionForOptimization(test_eval_return_last_set_property);
assert_test_eval_return_last_set_property(test_eval_return_last_set_property());
assert_test_eval_return_last_set_property(test_eval_return_last_set_property());
%OptimizeMaglevOnNextCall(test_eval_return_last_set_property);
assert_test_eval_return_last_set_property(test_eval_return_last_set_property());
assertOptimized(test_eval_return_last_set_property);
assertTrue(isMaglevved(test_eval_return_last_set_property));
assert_test_eval_return_last_set_property(test_eval_return_last_set_property());
%OptimizeFunctionOnNextCall(test_eval_return_last_set_property);
assert_test_eval_return_last_set_property(test_eval_return_last_set_property());
assertOptimized(test_eval_return_last_set_property);
assert_test_eval_return_last_set_property(test_eval_return_last_set_property());
}

run();
