// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

// Test: Setter on the prototype chain (Object.prototype).
// The optimization might bypass the setter
// and create an own property instead.

function test_setter_chain() {
  const key = "setter_prop_" + Math.random();
  let setterCalled = false;

  Object.defineProperty(Object.prototype, key, {
    set: function (val) {
      setterCalled = true;
    },
    get: function () {
      return "getter";
    },
    configurable: true,
  });

  function F() {}

  F.prototype[key] = "override";
  F.prototype.b = 2;

  // Cleanup
  delete Object.prototype[key];

  return { F, key, setterCalled };
}

function assert_test_setter_chain(result) {
  const { F, key, setterCalled } = result;

  // Assignment should call the setter.
  assertTrue(setterCalled, "Setter on prototype chain should be called");

  // F.prototype should NOT have own property 'key'.
  assertFalse(
    Object.hasOwn(F.prototype, key),
    "Should not have own property shadowing setter",
  );
  assertEquals(2, F.prototype.b);
}

// Run test
// prettier-ignore
function run(){
assert_test_setter_chain(test_setter_chain());
%CompileBaseline(test_setter_chain);
assert_test_setter_chain(test_setter_chain());
%PrepareFunctionForOptimization(test_setter_chain);
assert_test_setter_chain(test_setter_chain());
assert_test_setter_chain(test_setter_chain());
%OptimizeMaglevOnNextCall(test_setter_chain);
assert_test_setter_chain(test_setter_chain());
assertOptimized(test_setter_chain);
assertTrue(isMaglevved(test_setter_chain));
assert_test_setter_chain(test_setter_chain());
%OptimizeFunctionOnNextCall(test_setter_chain);
assert_test_setter_chain(test_setter_chain());
assertOptimized(test_setter_chain);
assert_test_setter_chain(test_setter_chain());
}

run();
