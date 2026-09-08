// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-fast-api --turbofan

const fast_c_api = new d8.test.FastCAPI();

// Direct negative input is correctly rejected.
{
  function testDirect(a) {
    return fast_c_api.sum_uint64_as_number(a, 0);
  }
  %PrepareFunctionForOptimization(testDirect);
  testDirect(1);
  testDirect(2);
  %OptimizeFunctionOnNextCall(testDirect);
  testDirect(3);

  fast_c_api.reset_counts();
  testDirect(4);
  assertTrue(fast_c_api.fast_call_count() > 0);

  assertThrows(() => testDirect(-1));
}

// Test that representation changes from TaggedSigned to Uint64 include proper
// deopt guards and reject negative numbers.
{
  function testTaggedSigned(a) {
    let x = a - 5;
    return fast_c_api.sum_uint64_as_number(x, 0);
  }
  %PrepareFunctionForOptimization(testTaggedSigned);
  for (let i = 1; i <= 3; i++) try {
      testTaggedSigned(i);
    } catch (e) {
    }
  %OptimizeFunctionOnNextCall(testTaggedSigned);
  try {
    testTaggedSigned(3);
  } catch (e) {
  }

  fast_c_api.reset_counts();
  testTaggedSigned(6);

  assertThrows(() => testTaggedSigned(4));
}

// Test that representation changes from Word32 to Uint64 include proper
// deopt guards and reject negative numbers.
{
  function testWord32(a) {
    let x = a | 0;
    return fast_c_api.sum_uint64_as_number(x, 0);
  }
  %PrepareFunctionForOptimization(testWord32);
  for (let i = -1; i >= -3; i--) try {
      testWord32(i);
    } catch (e) {
    }
  %OptimizeFunctionOnNextCall(testWord32);
  try {
    testWord32(-3);
  } catch (e) {
  }

  fast_c_api.reset_counts();
  testWord32(1);

  assertThrows(() => testWord32(-1));
}
