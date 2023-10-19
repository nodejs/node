// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sparkplug --no-always-sparkplug --sparkplug-filter="test*"
// Flags: --allow-natives-syntax --expose-gc --no-always-turbofan
// Flags: --baseline-batch-compilation --baseline-batch-compilation-threshold=200
// Flags: --invocation-count-for-feedback-allocation=4
// Flags: --no-concurrent-sparkplug

// Flags to drive Fuzzers into the right direction
// TODO(v8:11853): Remove these flags once fuzzers handle flag implications
// better.
// Flags: --lazy-feedback-allocation --no-stress-concurrent-inlining

// Basic test
(function() {
  // Bytecode length 24 -> estimated instruction size 120 - 168.
  function test1 (a,b) {
    return (a + b + 11) * 42 / a % b;
  }

  // Bytecode length 24 -> estimated instruction size 120 - 168.
  function test2 (a,b) {
    return (a + b + 11) * 42 / a % b;
  }

  %NeverOptimizeFunction(test1);
  // Trigger bytecode budget interrupt for test1.
  for (let i=0; i<5; ++i) {
    test1(i,4711);
  }
  // Shouldn't be compiled because of batch compilation.
  assertFalse(isBaseline(test1));

  %NeverOptimizeFunction(test2);
  // Trigger bytecode budget interrupt for test2.
  for (let i=0; i<5; ++i) {
    test2(i,4711);
  }

  // Call test1 again so baseline code gets installed on the function.
  test1(1,2);

  // Both functions should be compiled with baseline now.
  assertTrue(isBaseline(test1));
  assertTrue(isBaseline(test2));
})();

// Test function weak handle.
(function() {
  function test_weak (a,b) {
    return (a + b + 11) * 42 / a % b;
  }

  function test2 (a,b) {
    return (a + b + 11) * 42 / a % b;
  }

  %NeverOptimizeFunction(test_weak);
  for (let i=0; i<5; ++i) {
    test_weak(i,4711);
  }

  gc(); // GC should cause the handle to test_weak to be freed.

  %NeverOptimizeFunction(test2);
  // Trigger bytecode budget interrupt for test2.
  for (let i=0; i<5; ++i) {
    test2(i,4711);
  }

  assertTrue(isBaseline(test2));
})();
