// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_not_function() {
  var F = {};
  F.prototype = {};

  F.prototype.a = 1;
  F.prototype.b = 2;
  return F;
}

function assert_test_not_function(F) {
  assertEquals(1, F.prototype.a);
  assertEquals(2, F.prototype.b);
}

// prettier-ignore
function run(){
assert_test_not_function(test_not_function());
%CompileBaseline(test_not_function);
assert_test_not_function(test_not_function());
%PrepareFunctionForOptimization(test_not_function);
assert_test_not_function(test_not_function());
assert_test_not_function(test_not_function());
%OptimizeMaglevOnNextCall(test_not_function);
assert_test_not_function(test_not_function());
assertOptimized(test_not_function);
assertTrue(isMaglevved(test_not_function));
assert_test_not_function(test_not_function());
%OptimizeFunctionOnNextCall(test_not_function);
assert_test_not_function(test_not_function());
assertOptimized(test_not_function);
assert_test_not_function(test_not_function());
}

run();
