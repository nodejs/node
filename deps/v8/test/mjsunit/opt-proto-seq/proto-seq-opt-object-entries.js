// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --proto-assign-seq-lazy-func-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_object_entries() {
  function foo(){};
  foo.prototype.key_1 = function () {return "ok"};
  foo.prototype.key_2 = function () {};

  let res = [];
  for (const [key, value] of Object.entries(foo.prototype)) {
    res.push(`${key}: ${value}`);
    value.call();
  }
  return res;
}

function assert_test_object_entries(result) {
  assertEquals(result.length, 2);
}

// prettier-ignore
function run(){
assert_test_object_entries(test_object_entries());
%CompileBaseline(test_object_entries);
assert_test_object_entries(test_object_entries());
%PrepareFunctionForOptimization(test_object_entries);
assert_test_object_entries(test_object_entries());
assert_test_object_entries(test_object_entries());
%OptimizeMaglevOnNextCall(test_object_entries);
assert_test_object_entries(test_object_entries());
assertOptimized(test_object_entries);
assertTrue(isMaglevved(test_object_entries));
assert_test_object_entries(test_object_entries());
}

run();
