// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_slow_path_non_extensible() {
  function F() {}
  Object.preventExtensions(F.prototype);
  F.prototype.a = 1;
  F.prototype.b = 2;
  return F;
}

function assert_test_slow_path_non_extensible(F) {
  assertFalse(Object.hasOwn(F.prototype, "a"));
  assertFalse(Object.hasOwn(F.prototype, "b"));
}

// prettier-ignore
function run(){
assert_test_slow_path_non_extensible(test_slow_path_non_extensible());
%CompileBaseline(test_slow_path_non_extensible);
assert_test_slow_path_non_extensible(test_slow_path_non_extensible());
%PrepareFunctionForOptimization(test_slow_path_non_extensible);
assert_test_slow_path_non_extensible(test_slow_path_non_extensible());
assert_test_slow_path_non_extensible(test_slow_path_non_extensible());
%OptimizeMaglevOnNextCall(test_slow_path_non_extensible);
assert_test_slow_path_non_extensible(test_slow_path_non_extensible());
assertOptimized(test_slow_path_non_extensible);
assertTrue(isMaglevved(test_slow_path_non_extensible));
assert_test_slow_path_non_extensible(test_slow_path_non_extensible());
%OptimizeFunctionOnNextCall(test_slow_path_non_extensible);
assert_test_slow_path_non_extensible(test_slow_path_non_extensible());
assertOptimized(test_slow_path_non_extensible);
assert_test_slow_path_non_extensible(test_slow_path_non_extensible());
}

run();
