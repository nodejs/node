// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --proto-assign-seq-lazy-func-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_spreading(){
  function foo(){};
  foo.prototype.key_1 = function () {return "OK"};
  foo.prototype.key_2 = function () {return "OK"};
  const obj = {...foo.prototype};
  let res = [];
  for (const key in obj) {
    res.push(obj[key]());
  }
  return res;
}

function assert_test_spreading(result) {
  assertEquals(result.length, 2);
  assertEquals(result[0], "OK");
  assertEquals(result[1], "OK");
}

// prettier-ignore
function run(){
assert_test_spreading(test_spreading());
%CompileBaseline(test_spreading);
assert_test_spreading(test_spreading());
%PrepareFunctionForOptimization(test_spreading);
assert_test_spreading(test_spreading());
assert_test_spreading(test_spreading());
%OptimizeMaglevOnNextCall(test_spreading);
assert_test_spreading(test_spreading());
assertOptimized(test_spreading);
assertTrue(isMaglevved(test_spreading));
assert_test_spreading(test_spreading());
}

run();
