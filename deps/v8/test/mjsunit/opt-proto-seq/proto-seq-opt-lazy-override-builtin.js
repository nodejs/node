// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --proto-assign-seq-lazy-func-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_lazy_override_builtin(builtin_func) {
    const arr = [builtin_func];
    builtin_func.prototype.add = function () {};
    builtin_func.prototype.foo = "1";
    return new builtin_func(arr);
}

function assert_test_lazy_override_builtin(builtin_instance) {
  assertEquals(builtin_instance.foo,"1");
}

// prettier-ignore
function run(){
assert_test_lazy_override_builtin(test_lazy_override_builtin(Set));
%CompileBaseline(test_lazy_override_builtin);
assert_test_lazy_override_builtin(test_lazy_override_builtin(Set));
%PrepareFunctionForOptimization(test_lazy_override_builtin);
assert_test_lazy_override_builtin(test_lazy_override_builtin(Set));
assert_test_lazy_override_builtin(test_lazy_override_builtin(Set));
%OptimizeMaglevOnNextCall(test_lazy_override_builtin);
assert_test_lazy_override_builtin(test_lazy_override_builtin(Set));
assertOptimized(test_lazy_override_builtin);
assertTrue(isMaglevved(test_lazy_override_builtin));
assert_test_lazy_override_builtin(test_lazy_override_builtin(Set));
%OptimizeFunctionOnNextCall(test_lazy_override_builtin);
assert_test_lazy_override_builtin(test_lazy_override_builtin(Set));
assertOptimized(test_lazy_override_builtin);
assert_test_lazy_override_builtin(test_lazy_override_builtin(Set));
}

run();
