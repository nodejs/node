// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --proto-assign-seq-lazy-func-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_structured_clone() {
  function foo(){};
  foo.prototype.key_1 = function () {return "ok"};
  foo.prototype.key_2 = function () {};
  assertThrows(() => {
    const clone = d8.serializer.serialize(foo.prototype);
    return 0
  }, Error);
  return 1;
}

function assert_test_structured_clone(result) {
  assertEquals(result, 1);
}

// prettier-ignore
function run(){
assert_test_structured_clone(test_structured_clone());
%CompileBaseline(test_structured_clone);
assert_test_structured_clone(test_structured_clone());
%PrepareFunctionForOptimization(test_structured_clone);
assert_test_structured_clone(test_structured_clone());
assert_test_structured_clone(test_structured_clone());
%OptimizeMaglevOnNextCall(test_structured_clone);
assert_test_structured_clone(test_structured_clone());
assertOptimized(test_structured_clone);
assertTrue(isMaglevved(test_structured_clone));
assert_test_structured_clone(test_structured_clone());
%OptimizeFunctionOnNextCall(test_structured_clone);
assert_test_structured_clone(test_structured_clone());
assertOptimized(test_structured_clone);
assert_test_structured_clone(test_structured_clone());
}

run();
