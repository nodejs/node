// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_reassign_local() {
  let F = function () {};
  let oldF = F;

  F.prototype.a = ((F = function () {}), 1);
  F.prototype.b = 2;

  return { oldF, newF: F };
}

function assert_test_reassign_local(result) {
  const { oldF, newF } = result;

  // Check 'a' is on oldF
  assertEquals(1, oldF.prototype.a);
  assertFalse(Object.hasOwn(newF.prototype, "a"));

  // Check 'b' is on newF
  assertEquals(2, newF.prototype.b);
  assertFalse(Object.hasOwn(oldF.prototype, "b"));
}

// Run test
// prettier-ignore
function run(){
assert_test_reassign_local(test_reassign_local());
%CompileBaseline(test_reassign_local);
assert_test_reassign_local(test_reassign_local());
%PrepareFunctionForOptimization(test_reassign_local);
assert_test_reassign_local(test_reassign_local());
assert_test_reassign_local(test_reassign_local());
%OptimizeMaglevOnNextCall(test_reassign_local);
assert_test_reassign_local(test_reassign_local());
assertOptimized(test_reassign_local);
assertTrue(isMaglevved(test_reassign_local));
assert_test_reassign_local(test_reassign_local());
%OptimizeFunctionOnNextCall(test_reassign_local);
assert_test_reassign_local(test_reassign_local());
assertOptimized(test_reassign_local);
assert_test_reassign_local(test_reassign_local());
}

run();
