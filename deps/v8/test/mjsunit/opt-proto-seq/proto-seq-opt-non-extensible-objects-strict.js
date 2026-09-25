// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_non_extensible_objects() {
  "use strict";
  function test_function() {}

  Object.preventExtensions(test_function.prototype);
  assertThrows(() => {
    test_function.prototype.smi_1 = 1;
    test_function.prototype.smi_2 = 2;
  }, TypeError);
  return new test_function();
}

function assert_test_non_extensible_objects(test_object) {
  assertEquals(test_object.smi_1, undefined);
  assertEquals(test_object.smi_2, undefined);
}

// prettier-ignore
function run(){
assert_test_non_extensible_objects(test_non_extensible_objects());
%CompileBaseline(test_non_extensible_objects);
assert_test_non_extensible_objects(test_non_extensible_objects());
%PrepareFunctionForOptimization(test_non_extensible_objects);
assert_test_non_extensible_objects(test_non_extensible_objects());
assert_test_non_extensible_objects(test_non_extensible_objects());
%OptimizeMaglevOnNextCall(test_non_extensible_objects);
assert_test_non_extensible_objects(test_non_extensible_objects());
assertOptimized(test_non_extensible_objects);
assertTrue(isMaglevved(test_non_extensible_objects));
assert_test_non_extensible_objects(test_non_extensible_objects());
%OptimizeFunctionOnNextCall(test_non_extensible_objects);
assert_test_non_extensible_objects(test_non_extensible_objects());
assertOptimized(test_non_extensible_objects);
assert_test_non_extensible_objects(test_non_extensible_objects());
}

run();
