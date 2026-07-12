// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_prototype_read_only() {
  function test_function() {}

  Object.defineProperty(test_function.prototype, "key", {
    value: 0,
    writable: false,
  });

  test_function.prototype.func = function () {
    return "test_function.prototype.func";
  };
  test_function.prototype.arrow_func = () => {
    return "test_function.prototype.arrow_func";
  };
  test_function.prototype.smi = 1;
  test_function.prototype.str = "test_function.prototype.str";
  test_function.prototype.key = 1;

  return new test_function();
}

function assert_test_prototype_read_only(test_instance) {
  assertEquals(test_instance.func(), "test_function.prototype.func");
  assertEquals(
    test_instance.arrow_func(),
    "test_function.prototype.arrow_func",
  );
  assertEquals(test_instance.smi, 1);
  assertEquals(test_instance.str, "test_function.prototype.str");
  assertEquals(test_instance.key, 0);
}

// prettier-ignore
function run(){
assert_test_prototype_read_only(test_prototype_read_only());
%CompileBaseline(test_prototype_read_only);
assert_test_prototype_read_only(test_prototype_read_only());
%PrepareFunctionForOptimization(test_prototype_read_only);
assert_test_prototype_read_only(test_prototype_read_only());
assert_test_prototype_read_only(test_prototype_read_only());
%OptimizeMaglevOnNextCall(test_prototype_read_only);
assert_test_prototype_read_only(test_prototype_read_only());
assertOptimized(test_prototype_read_only);
assertTrue(isMaglevved(test_prototype_read_only));
assert_test_prototype_read_only(test_prototype_read_only());
%OptimizeFunctionOnNextCall(test_prototype_read_only);
assert_test_prototype_read_only(test_prototype_read_only());
assertOptimized(test_prototype_read_only);
assert_test_prototype_read_only(test_prototype_read_only());
}

run();
