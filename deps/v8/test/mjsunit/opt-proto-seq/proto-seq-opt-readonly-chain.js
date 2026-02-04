// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_readonly_chain() {
  const key = "readonly_prop_" + Math.random();
  Object.defineProperty(Object.prototype, key, {
    value: "read-only",
    writable: false,
    configurable: true,
  });

  function F() {}
  F.prototype[key] = "override";
  F.prototype.b = 2;

  return [F, key];
}

function assert_test_readonly_chain(result) {
  F = result[0];
  key = result[1];

  // Verify immediately before cleanup
  // In sloppy mode, assignment to read-only fails silently.
  // F.prototype should NOT have own property 'key'.
  // F.prototype[key] should be 'read-only'.
  assertFalse(
    Object.hasOwn(F.prototype, key),
    "Should not have own property shadowing read-only proto property",
  );
  assertEquals("read-only", F.prototype[key]);
  assertEquals(2, F.prototype.b);

  // Cleanup to avoid polluting other tests
  delete Object.prototype[key];
}

// Run test
// prettier-ignore
function run(){
assert_test_readonly_chain(test_readonly_chain());
%CompileBaseline(test_readonly_chain);
assert_test_readonly_chain(test_readonly_chain());
%PrepareFunctionForOptimization(test_readonly_chain);
assert_test_readonly_chain(test_readonly_chain());
assert_test_readonly_chain(test_readonly_chain());
%OptimizeMaglevOnNextCall(test_readonly_chain);
assert_test_readonly_chain(test_readonly_chain());
assertOptimized(test_readonly_chain);
assertTrue(isMaglevved(test_readonly_chain));
assert_test_readonly_chain(test_readonly_chain());
%OptimizeFunctionOnNextCall(test_readonly_chain);
assert_test_readonly_chain(test_readonly_chain());
assertOptimized(test_readonly_chain);
assert_test_readonly_chain(test_readonly_chain());
}

run();
