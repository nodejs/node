// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_ignore_chain_descriptors() {
  function mock(type) {
    type.prototype.fill = function (value, start, end) {};
    type.prototype.key = 0;
    return type.prototype;
  }
  return mock(Float64Array);
}

function assert_test_ignore_chain_descriptors(type) {
  // While own descriptor should be preserved when overriding value,
  // Parent ones are to be ignored
  assertEquals(Object.getOwnPropertyDescriptor(type, 'fill').enumerable, true);

  assertEquals(Object.getPrototypeOf(type).constructor.name, 'TypedArray');

  assertEquals(
    Object.getOwnPropertyDescriptor(Object.getPrototypeOf(type), 'fill')
      .enumerable,
    false
  );
}

// prettier-ignore
function run(){
  assert_test_ignore_chain_descriptors(test_ignore_chain_descriptors());
  %CompileBaseline(test_ignore_chain_descriptors);
  assert_test_ignore_chain_descriptors(test_ignore_chain_descriptors());
  %PrepareFunctionForOptimization(test_ignore_chain_descriptors);
  assert_test_ignore_chain_descriptors(test_ignore_chain_descriptors());
  assert_test_ignore_chain_descriptors(test_ignore_chain_descriptors());
  %OptimizeMaglevOnNextCall(test_ignore_chain_descriptors);
  assert_test_ignore_chain_descriptors(test_ignore_chain_descriptors());
  assertOptimized(test_ignore_chain_descriptors);
  assertTrue(isMaglevved(test_ignore_chain_descriptors));
  assert_test_ignore_chain_descriptors(test_ignore_chain_descriptors());
  %OptimizeFunctionOnNextCall(test_ignore_chain_descriptors);
  assert_test_ignore_chain_descriptors(test_ignore_chain_descriptors());
  assertOptimized(test_ignore_chain_descriptors);
  assert_test_ignore_chain_descriptors(test_ignore_chain_descriptors());
}
run();
