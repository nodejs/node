// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_functions_optimization() {
  function F() {}
  F.prototype.func1 = function () {
    return 1;
  };
  F.prototype.func2 = function () {
    return 2;
  };
  return F;
}

function assert_test_functions_optimization(F) {
  const f = new F();
  assertEquals(1, f.func1());
  assertEquals(2, f.func2());
}

// prettier-ignore
function run(){
assert_test_functions_optimization(test_functions_optimization());
%CompileBaseline(test_functions_optimization);
assert_test_functions_optimization(test_functions_optimization());
%PrepareFunctionForOptimization(test_functions_optimization);
assert_test_functions_optimization(test_functions_optimization());
assert_test_functions_optimization(test_functions_optimization());
%OptimizeMaglevOnNextCall(test_functions_optimization);
assert_test_functions_optimization(test_functions_optimization());
assertOptimized(test_functions_optimization);
assertTrue(isMaglevved(test_functions_optimization));
assert_test_functions_optimization(test_functions_optimization());
%OptimizeFunctionOnNextCall(test_functions_optimization);
assert_test_functions_optimization(test_functions_optimization());
assertOptimized(test_functions_optimization);
assert_test_functions_optimization(test_functions_optimization());
}

run();
