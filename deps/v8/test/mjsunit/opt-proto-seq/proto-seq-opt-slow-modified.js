// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_slow_path_modified_proto() {
  function F() {}
  // Add a property to prevent fast path (IsDefaultFunctionPrototype)
  Object.defineProperty(F.prototype, "existing", {
    value: 10,
    writable: true,
    configurable: true,
    enumerable: true,
  });

  F.prototype.a = 1;
  F.prototype.b = 2;
  return F;
}

function assert_test_slow_path_modified_proto(F) {
  assertEquals(1, F.prototype.a);
  assertEquals(2, F.prototype.b);
  assertEquals(10, F.prototype.existing);
}

// prettier-ignore
function run(){
assert_test_slow_path_modified_proto(test_slow_path_modified_proto());
%CompileBaseline(test_slow_path_modified_proto);
assert_test_slow_path_modified_proto(test_slow_path_modified_proto());
%PrepareFunctionForOptimization(test_slow_path_modified_proto);
assert_test_slow_path_modified_proto(test_slow_path_modified_proto());
assert_test_slow_path_modified_proto(test_slow_path_modified_proto());
%OptimizeMaglevOnNextCall(test_slow_path_modified_proto);
assert_test_slow_path_modified_proto(test_slow_path_modified_proto());
assertOptimized(test_slow_path_modified_proto);
assertTrue(isMaglevved(test_slow_path_modified_proto));
assert_test_slow_path_modified_proto(test_slow_path_modified_proto());
%OptimizeFunctionOnNextCall(test_slow_path_modified_proto);
assert_test_slow_path_modified_proto(test_slow_path_modified_proto());
assertOptimized(test_slow_path_modified_proto);
assert_test_slow_path_modified_proto(test_slow_path_modified_proto());
}

run();
