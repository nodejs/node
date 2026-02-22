// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_frozen_objects() {
  function test_function() {}

  Object["freeze"](test_function.prototype);
  test_function.prototype.smi_1 = 1;
  test_function.prototype.smi_2 = 2;
  return new test_function();
}

function assert_test_frozen_objects(test_object) {
  assertEquals(test_object.smi_1, undefined);
  assertEquals(test_object.smi_2, undefined);
}

// prettier-ignore
function run(){
assert_test_frozen_objects(test_frozen_objects());
%CompileBaseline(test_frozen_objects);
assert_test_frozen_objects(test_frozen_objects());
%PrepareFunctionForOptimization(test_frozen_objects);
assert_test_frozen_objects(test_frozen_objects());
assert_test_frozen_objects(test_frozen_objects());
%OptimizeMaglevOnNextCall(test_frozen_objects);
assert_test_frozen_objects(test_frozen_objects());
assertOptimized(test_frozen_objects);
assertTrue(isMaglevved(test_frozen_objects));
assert_test_frozen_objects(test_frozen_objects());
%OptimizeFunctionOnNextCall(test_frozen_objects);
assert_test_frozen_objects(test_frozen_objects());
assertOptimized(test_frozen_objects);
assert_test_frozen_objects(test_frozen_objects());
}

run();
