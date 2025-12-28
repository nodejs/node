// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_prototype_proto_keys() {
  function test_function() {}
  var das_proto = {};
  Object.defineProperty(das_proto, "smi", {
    set(x) {
      test_function.prototype.str = "foo";
    },
    get() {
      return 0;
    },
  });

  test_function.prototype.func = function () {
    return "test_function.prototype.func";
  };
  test_function.prototype.arrow_func = () => {
    return "test_function.prototype.arrow_func";
  };
  test_function.prototype.__proto__ = das_proto;
  test_function.prototype.str = "test_function.prototype.str";
  test_function.prototype.smi = 1;

  return new test_function();
}

function assert_test_prototype_proto_keys(test_instance) {
  assertEquals(test_instance.func(), "test_function.prototype.func");
  assertEquals(
    test_instance.arrow_func(),
    "test_function.prototype.arrow_func",
  );
  assertEquals(test_instance.smi, 0);
  assertEquals(test_instance.str, "foo");
}

// prettier-ignore
function run(){
assert_test_prototype_proto_keys(test_prototype_proto_keys());
%CompileBaseline(test_prototype_proto_keys);
assert_test_prototype_proto_keys(test_prototype_proto_keys());
%PrepareFunctionForOptimization(test_prototype_proto_keys);
assert_test_prototype_proto_keys(test_prototype_proto_keys());
assert_test_prototype_proto_keys(test_prototype_proto_keys());
%OptimizeMaglevOnNextCall(test_prototype_proto_keys);
assert_test_prototype_proto_keys(test_prototype_proto_keys());
assertOptimized(test_prototype_proto_keys);
assertTrue(isMaglevved(test_prototype_proto_keys));
assert_test_prototype_proto_keys(test_prototype_proto_keys());
%OptimizeFunctionOnNextCall(test_prototype_proto_keys);
assert_test_prototype_proto_keys(test_prototype_proto_keys());
assertOptimized(test_prototype_proto_keys);
assert_test_prototype_proto_keys(test_prototype_proto_keys());
}

run();
