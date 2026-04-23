// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_strict_mode() {
  "use strict";
  function F() {}
  F.prototype.a = 1;
  F.prototype.b = 2;
  return F;
}

function assert_test_strict_mode(F) {
  assertEquals(1, F.prototype.a);
  assertEquals(2, F.prototype.b);
}

// prettier-ignore
function run(){
assert_test_strict_mode(test_strict_mode());
%CompileBaseline(test_strict_mode);
assert_test_strict_mode(test_strict_mode());
%PrepareFunctionForOptimization(test_strict_mode);
assert_test_strict_mode(test_strict_mode());
assert_test_strict_mode(test_strict_mode());
%OptimizeMaglevOnNextCall(test_strict_mode);
assert_test_strict_mode(test_strict_mode());
assertOptimized(test_strict_mode);
assertTrue(isMaglevved(test_strict_mode));
assert_test_strict_mode(test_strict_mode());
%OptimizeFunctionOnNextCall(test_strict_mode);
assert_test_strict_mode(test_strict_mode());
assertOptimized(test_strict_mode);
assert_test_strict_mode(test_strict_mode());
}

run();
