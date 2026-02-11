// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_preserve_descriptor() {
  function test_function() {}
  Object.defineProperty(test_function.prototype, "prop_with_desc", {
    configurable: false,
    writable: true,
  });

  test_function.prototype.x = 1;
  test_function.prototype.prop_with_desc = 2;

  return new test_function();
}

function assert_test_preserve_descriptor(obj) {
  assertEquals(obj.x, 1);
  assertEquals(obj.prop_with_desc, 2);
  let desc = Object.getOwnPropertyDescriptor(obj.__proto__, "prop_with_desc");
  assertEquals(desc.writable, true);
  assertEquals(desc.configurable, false);
}

// prettier-ignore
function run(){
assert_test_preserve_descriptor(test_preserve_descriptor());
%CompileBaseline(test_preserve_descriptor);
assert_test_preserve_descriptor(test_preserve_descriptor());
%PrepareFunctionForOptimization(test_preserve_descriptor);
assert_test_preserve_descriptor(test_preserve_descriptor());
assert_test_preserve_descriptor(test_preserve_descriptor());
%OptimizeMaglevOnNextCall(test_preserve_descriptor);
assert_test_preserve_descriptor(test_preserve_descriptor());
assertOptimized(test_preserve_descriptor);
assertTrue(isMaglevved(test_preserve_descriptor));
assert_test_preserve_descriptor(test_preserve_descriptor());
%OptimizeFunctionOnNextCall(test_preserve_descriptor);
assert_test_preserve_descriptor(test_preserve_descriptor());
assertOptimized(test_preserve_descriptor);
assert_test_preserve_descriptor(test_preserve_descriptor());
}

run();
