// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_not_proto_assign_seq() {
  function test_function() {}
  test_function.prototype = [];

  test_function.prototype["0"] = function () {
    return "test_function.prototype.func";
  };
  test_function.prototype["1"] = () => {
    return "test_function.prototype.arrow_func";
  };
  test_function.prototype["2"] = 1;

  return test_function;
}

function assert_test_not_proto_assign_seq(test_function) {
  assertEquals(test_function.prototype["0"](), "test_function.prototype.func");
  assertEquals(
    test_function.prototype["1"](),
    "test_function.prototype.arrow_func",
  );
  assertEquals(test_function.prototype["2"], 1);
}

// prettier-ignore
function run(){
assert_test_not_proto_assign_seq(test_not_proto_assign_seq());
%CompileBaseline(test_not_proto_assign_seq);
assert_test_not_proto_assign_seq(test_not_proto_assign_seq());
%PrepareFunctionForOptimization(test_not_proto_assign_seq);
assert_test_not_proto_assign_seq(test_not_proto_assign_seq());
assert_test_not_proto_assign_seq(test_not_proto_assign_seq());
%OptimizeMaglevOnNextCall(test_not_proto_assign_seq);
assert_test_not_proto_assign_seq(test_not_proto_assign_seq());
assertOptimized(test_not_proto_assign_seq);
assertTrue(isMaglevved(test_not_proto_assign_seq));
assert_test_not_proto_assign_seq(test_not_proto_assign_seq());
%OptimizeFunctionOnNextCall(test_not_proto_assign_seq);
assert_test_not_proto_assign_seq(test_not_proto_assign_seq());
assertOptimized(test_not_proto_assign_seq);
assert_test_not_proto_assign_seq(test_not_proto_assign_seq());
}

run();
