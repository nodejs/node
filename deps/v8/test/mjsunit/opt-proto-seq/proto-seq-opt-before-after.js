// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_before_after() {
  function test_function() {}
  let before = undefined;
  let after = undefined;

  before = "before";
  test_function.prototype.x = 1;
  test_function.prototype.y = 2;
  after = "after";

  return [new test_function(), before, after];
}

function assert_test_before_after(arr) {
  assertEquals(arr[0].x, 1);
  assertEquals(arr[0].y, 2);
  assertEquals(arr[1], "before");
  assertEquals(arr[2], "after");
}

// prettier-ignore
function run(){
assert_test_before_after(test_before_after());
%CompileBaseline(test_before_after);
assert_test_before_after(test_before_after());
%PrepareFunctionForOptimization(test_before_after);
assert_test_before_after(test_before_after());
assert_test_before_after(test_before_after());
%OptimizeMaglevOnNextCall(test_before_after);
assert_test_before_after(test_before_after());
assertOptimized(test_before_after);
assertTrue(isMaglevved(test_before_after));
assert_test_before_after(test_before_after());
%OptimizeFunctionOnNextCall(test_before_after);
assert_test_before_after(test_before_after());
assertOptimized(test_before_after);
assert_test_before_after(test_before_after());
}

run();
