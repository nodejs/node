// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --proto-assign-seq-lazy-func-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_destructuring(){
  function foo(){};
  foo.prototype.key_1 = function () {return "OK"};
  foo.prototype.key_2 = function () {return "OK"};
  const { key_1, key_2 } =  foo.prototype;
  return [key_1(), key_2()];
}

function assert_test_destructuring(result) {
  assertEquals(result.length, 2);
  assertEquals(result[0], "OK");
  assertEquals(result[1], "OK");
}

// prettier-ignore
function run(){
assert_test_destructuring(test_destructuring());
%CompileBaseline(test_destructuring);
assert_test_destructuring(test_destructuring());
%PrepareFunctionForOptimization(test_destructuring);
assert_test_destructuring(test_destructuring());
assert_test_destructuring(test_destructuring());
}

run();
