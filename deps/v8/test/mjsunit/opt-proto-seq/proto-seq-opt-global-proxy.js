// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_global_proxy() {
  function test(){}

  test.prototype.f1 = function () { return "OK"};
  test.prototype.f2 = function () {};


  globalThis.__proto__ = test.prototype;
  return globalThis.f1();
}

function assert_test_global_proxy(status) {
  assertEquals(status, "OK");
}

// prettier-ignore
function run(){
  assert_test_global_proxy(test_global_proxy());
  %CompileBaseline(test_global_proxy);
  assert_test_global_proxy(test_global_proxy());
  %PrepareFunctionForOptimization(test_global_proxy);
  assert_test_global_proxy(test_global_proxy());
  assert_test_global_proxy(test_global_proxy());
  %OptimizeMaglevOnNextCall(test_global_proxy);
  assert_test_global_proxy(test_global_proxy());
  assertOptimized(test_global_proxy);
  assertTrue(isMaglevved(test_global_proxy));
  assert_test_global_proxy(test_global_proxy());
  %OptimizeFunctionOnNextCall(test_global_proxy);
  assert_test_global_proxy(test_global_proxy());
  assertOptimized(test_global_proxy);
  assert_test_global_proxy(test_global_proxy());
}

run();
