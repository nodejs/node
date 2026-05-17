// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_proto_of_prototype_assigned() {
  function test_function() {}

  test_function.prototype.func = function () {
    return "test_function.prototype.func";
  };
  test_function.prototype.__proto__ = Object.prototype.__proto__;
  test_function.prototype.smi = 1;

  return new test_function();
}

function assert_test_proto_of_prototype_assigned(test_instance) {
  assertEquals(test_instance.func(), "test_function.prototype.func");
  assertEquals(test_instance.smi, 1);
}

// prettier-ignore
function run(){
assert_test_proto_of_prototype_assigned(test_proto_of_prototype_assigned());
%CompileBaseline(test_proto_of_prototype_assigned);
assert_test_proto_of_prototype_assigned(test_proto_of_prototype_assigned());
%PrepareFunctionForOptimization(test_proto_of_prototype_assigned);
assert_test_proto_of_prototype_assigned(test_proto_of_prototype_assigned());
assert_test_proto_of_prototype_assigned(test_proto_of_prototype_assigned());
%OptimizeMaglevOnNextCall(test_proto_of_prototype_assigned);
assert_test_proto_of_prototype_assigned(test_proto_of_prototype_assigned());
assertOptimized(test_proto_of_prototype_assigned);
assertTrue(isMaglevved(test_proto_of_prototype_assigned));
assert_test_proto_of_prototype_assigned(test_proto_of_prototype_assigned());
%OptimizeFunctionOnNextCall(test_proto_of_prototype_assigned);
assert_test_proto_of_prototype_assigned(test_proto_of_prototype_assigned());
assertOptimized(test_proto_of_prototype_assigned);
assert_test_proto_of_prototype_assigned(test_proto_of_prototype_assigned());
}

run();
