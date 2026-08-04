// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt --proto-assign-seq-opt-count=10
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_count_limit() {
  function F() {}
  F.prototype.a = 1;
  F.prototype.b = 2;
  F.prototype.c = 3;
  F.prototype.d = 4;
  return F;
}

function assert_test_count_limit(F) {
  assertEquals(1, F.prototype.a);
  assertEquals(2, F.prototype.b);
  assertEquals(3, F.prototype.c);
  assertEquals(4, F.prototype.d);
}

// prettier-ignore
function run(){
assert_test_count_limit(test_count_limit());
%CompileBaseline(test_count_limit);
assert_test_count_limit(test_count_limit());
%PrepareFunctionForOptimization(test_count_limit);
assert_test_count_limit(test_count_limit());
assert_test_count_limit(test_count_limit());
%OptimizeMaglevOnNextCall(test_count_limit);
assert_test_count_limit(test_count_limit());
assertOptimized(test_count_limit);
assertTrue(isMaglevved(test_count_limit));
assert_test_count_limit(test_count_limit());
%OptimizeFunctionOnNextCall(test_count_limit);
assert_test_count_limit(test_count_limit());
assertOptimized(test_count_limit);
assert_test_count_limit(test_count_limit());
}

run();
