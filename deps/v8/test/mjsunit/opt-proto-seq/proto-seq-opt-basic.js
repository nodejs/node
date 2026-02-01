// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_basic_optimization() {
  function F() {}
  F.prototype.a = 1;
  F.prototype.b = 2;
  F.prototype.c = "string";
  F.prototype.d = null;
  return F;
}

function assert_test_basic_optimization(F) {
  const f = new F();
  assertEquals(1, f.a);
  assertEquals(2, f.b);
  assertEquals("string", f.c);
  assertEquals(null, f.d);
  assertEquals(1, F.prototype.a);
  assertEquals(2, F.prototype.b);
  assertEquals("string", F.prototype.c);
  assertEquals(null, F.prototype.d);

  // Verify descriptor properties (default: writable, enumerable, configurable)
  let desc = Object.getOwnPropertyDescriptor(F.prototype, "a");
  assertTrue(desc.writable);
  assertTrue(desc.enumerable);
  assertTrue(desc.configurable);
}

// prettier-ignore
function run(){
assert_test_basic_optimization(test_basic_optimization());
%CompileBaseline(test_basic_optimization);
assert_test_basic_optimization(test_basic_optimization());
%PrepareFunctionForOptimization(test_basic_optimization);
assert_test_basic_optimization(test_basic_optimization());
assert_test_basic_optimization(test_basic_optimization());
%OptimizeMaglevOnNextCall(test_basic_optimization);
assert_test_basic_optimization(test_basic_optimization());
assertOptimized(test_basic_optimization);
assertTrue(isMaglevved(test_basic_optimization));
assert_test_basic_optimization(test_basic_optimization());
%OptimizeFunctionOnNextCall(test_basic_optimization);
assert_test_basic_optimization(test_basic_optimization());
assertOptimized(test_basic_optimization);
assert_test_basic_optimization(test_basic_optimization());
}

run();
