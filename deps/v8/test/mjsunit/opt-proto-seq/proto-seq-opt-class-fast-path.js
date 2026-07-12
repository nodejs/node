// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_class_fast_path() {
  class test_class {
    constructor() {}
  }

  test_class.prototype.func = function () {
    return "test_class.prototype.func";
  };
  test_class.prototype.arrow_func = () => {
    return "test_class.prototype.arrow_func";
  };
  test_class.prototype.smi = 1;
  test_class.prototype.str = "test_class.prototype.str";
  // TODO(rherouart): handle object and array literals
  // test_class.prototype.obj = {o:{smi:1, str:"str"}, smi:0};
  // test_class.prototype.arr = [0,1,2];
  return new test_class();
}

function assert_test_class_fast_path(test_instance) {
  assertEquals(test_instance.func(), "test_class.prototype.func");
  assertEquals(test_instance.arrow_func(), "test_class.prototype.arrow_func");
  assertEquals(test_instance.smi, 1);
  assertEquals(test_instance.str, "test_class.prototype.str");
}

// prettier-ignore
function run(){
assert_test_class_fast_path(test_class_fast_path());
%CompileBaseline(test_class_fast_path);
assert_test_class_fast_path(test_class_fast_path());
%PrepareFunctionForOptimization(test_class_fast_path);
assert_test_class_fast_path(test_class_fast_path());
assert_test_class_fast_path(test_class_fast_path());
%OptimizeMaglevOnNextCall(test_class_fast_path);
assert_test_class_fast_path(test_class_fast_path());
assertOptimized(test_class_fast_path);
assertTrue(isMaglevved(test_class_fast_path));
assert_test_class_fast_path(test_class_fast_path());
%OptimizeFunctionOnNextCall(test_class_fast_path);
assert_test_class_fast_path(test_class_fast_path());
assertOptimized(test_class_fast_path);
assert_test_class_fast_path(test_class_fast_path());
}

run();
