// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_locked_proto() {
  "use strict";
  function test_function() {}
  test_function.prototype = {
    __proto__: Object.freeze({
      lock: 1,
    }),
    before: 2,
    after: 3,
  };

  assertThrows(() => {
    test_function.prototype.before = 4;
    test_function.prototype.lock = 2;
    test_function.prototype.after = 5;
  }, TypeError);

  return new test_function();
}

function assert_test_locked_proto(obj) {
  assertEquals(obj.lock, 1);
  assertEquals(obj.before, 4);
  assertEquals(obj.after, 3);
}

// prettier-ignore
function run(){
assert_test_locked_proto(test_locked_proto());
%CompileBaseline(test_locked_proto);
assert_test_locked_proto(test_locked_proto());
%PrepareFunctionForOptimization(test_locked_proto);
assert_test_locked_proto(test_locked_proto());
assert_test_locked_proto(test_locked_proto());
%OptimizeMaglevOnNextCall(test_locked_proto);
assert_test_locked_proto(test_locked_proto());
assertOptimized(test_locked_proto);
assertTrue(isMaglevved(test_locked_proto));
assert_test_locked_proto(test_locked_proto());
%OptimizeFunctionOnNextCall(test_locked_proto);
assert_test_locked_proto(test_locked_proto());
assertOptimized(test_locked_proto);
assert_test_locked_proto(test_locked_proto());
}

run();
