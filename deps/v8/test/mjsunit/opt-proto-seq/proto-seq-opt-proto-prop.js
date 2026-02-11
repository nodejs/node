// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_proto_assignment() {
  function F() {}
  F.prototype.a = 1;
  // Assignment to __proto__ is excluded from optimization.
  F.prototype.__proto__ = { x: 10 };
  F.prototype.b = 2;
  return F;
}

function assert_test_proto_assignment(F) {
  assertEquals(1, F.prototype.a);
  assertEquals(10, F.prototype.__proto__.x);
  assertEquals(2, F.prototype.b);
}

// prettier-ignore
function run(){
assert_test_proto_assignment(test_proto_assignment());
%CompileBaseline(test_proto_assignment);
assert_test_proto_assignment(test_proto_assignment());
%PrepareFunctionForOptimization(test_proto_assignment);
assert_test_proto_assignment(test_proto_assignment());
assert_test_proto_assignment(test_proto_assignment());
%OptimizeMaglevOnNextCall(test_proto_assignment);
assert_test_proto_assignment(test_proto_assignment());
assertOptimized(test_proto_assignment);
assertTrue(isMaglevved(test_proto_assignment));
assert_test_proto_assignment(test_proto_assignment());
%OptimizeFunctionOnNextCall(test_proto_assignment);
assert_test_proto_assignment(test_proto_assignment());
assertOptimized(test_proto_assignment);
assert_test_proto_assignment(test_proto_assignment());
}

run();
