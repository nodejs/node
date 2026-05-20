// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_variable_proxy() {
  var calls = 0;
  let foo = {
    get prototype() {
      calls += 1;
      foo = {};
      return { prototype: {} };
    },
  };
  assertThrows(() => {
    foo.prototype.k1 = 1;
    foo.prototype.k2 = 2;
  }, TypeError);

  return [calls, foo];
}

function assert_test_variable_proxy(arr) {
  assertEquals(arr[0], 1);
  assertEquals(Object.keys(arr[1]).length, 0);
}

// prettier-ignore
function run(){
assert_test_variable_proxy(test_variable_proxy());
%CompileBaseline(test_variable_proxy);
assert_test_variable_proxy(test_variable_proxy());
%PrepareFunctionForOptimization(test_variable_proxy);
assert_test_variable_proxy(test_variable_proxy());
assert_test_variable_proxy(test_variable_proxy());
%OptimizeMaglevOnNextCall(test_variable_proxy);
assert_test_variable_proxy(test_variable_proxy());
assertOptimized(test_variable_proxy);
assertTrue(isMaglevved(test_variable_proxy));
assert_test_variable_proxy(test_variable_proxy());
%OptimizeFunctionOnNextCall(test_variable_proxy);
assert_test_variable_proxy(test_variable_proxy());
assertOptimized(test_variable_proxy);
assert_test_variable_proxy(test_variable_proxy());
}

run();
