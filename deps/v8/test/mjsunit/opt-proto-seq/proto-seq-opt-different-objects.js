// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_different_objects() {
  function F() {}
  function G() {}
  F.prototype.a = 1;
  G.prototype.b = 2;  // Different object
  F.prototype.c = 3;
  return { F, G };
}

function assert_test_different_objects(res) {
  const F = res.F;
  const G = res.G;
  assertEquals(1, F.prototype.a);
  assertEquals(2, G.prototype.b);
  assertEquals(3, F.prototype.c);
}

// prettier-ignore
function run(){
assert_test_different_objects(test_different_objects());
%CompileBaseline(test_different_objects);
assert_test_different_objects(test_different_objects());
%PrepareFunctionForOptimization(test_different_objects);
assert_test_different_objects(test_different_objects());
assert_test_different_objects(test_different_objects());
%OptimizeMaglevOnNextCall(test_different_objects);
assert_test_different_objects(test_different_objects());
assertOptimized(test_different_objects);
assertTrue(isMaglevved(test_different_objects));
assert_test_different_objects(test_different_objects());
%OptimizeFunctionOnNextCall(test_different_objects);
assert_test_different_objects(test_different_objects());
assertOptimized(test_different_objects);
assert_test_different_objects(test_different_objects());
}

run();
