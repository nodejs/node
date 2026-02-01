// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_has_prototype_keys() {
  const test_object = {};
  Object.defineProperty(test_object, "prototype", {
    value: {},
  });

  test_object.prototype.func = function () {
    return "test_object.prototype.func";
  };
  test_object.prototype.arrow_func = () => {
    return "test_object.prototype.arrow_func";
  };
  test_object.prototype.smi = 1;
  test_object.prototype.str = "test_object.prototype.str";

  return test_object;
}

function assert_test_has_prototype_keys(test_object) {
  assertEquals(test_object.prototype.func(), "test_object.prototype.func");
  assertEquals(
    test_object.prototype.arrow_func(),
    "test_object.prototype.arrow_func",
  );
  assertEquals(test_object.prototype.smi, 1);
  assertEquals(test_object.prototype.str, "test_object.prototype.str");
}

// prettier-ignore
function run(){
assert_test_has_prototype_keys(test_has_prototype_keys());
%CompileBaseline(test_has_prototype_keys);
assert_test_has_prototype_keys(test_has_prototype_keys());
%PrepareFunctionForOptimization(test_has_prototype_keys);
assert_test_has_prototype_keys(test_has_prototype_keys());
assert_test_has_prototype_keys(test_has_prototype_keys());
%OptimizeMaglevOnNextCall(test_has_prototype_keys);
assert_test_has_prototype_keys(test_has_prototype_keys());
assertOptimized(test_has_prototype_keys);
assertTrue(isMaglevved(test_has_prototype_keys));
assert_test_has_prototype_keys(test_has_prototype_keys());
%OptimizeFunctionOnNextCall(test_has_prototype_keys);
assert_test_has_prototype_keys(test_has_prototype_keys());
assertOptimized(test_has_prototype_keys);
assert_test_has_prototype_keys(test_has_prototype_keys());
}

run();
