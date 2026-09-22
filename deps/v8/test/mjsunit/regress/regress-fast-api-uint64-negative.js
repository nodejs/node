// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-fast-api --turbo-fast-api-calls
// Flags: --deopt-every-n-times=0 --turbofan

const fast_c_api = new d8.test.FastCAPI();

// Test that Float64 to Uint64 representation change includes proper deopt
// checks when the input type has a negative static range (subtype of Int64).
{
  function testFloat64NegativeRange(v) {
    let val = (v | 0) + 0.1;
    val = Math.floor(val);
    val = Math.max(-10, Math.min(-1, val));
    return fast_c_api.sum_uint64_as_number(val, 0);
  }

  %PrepareFunctionForOptimization(testFloat64NegativeRange);
  assertThrows(() => testFloat64NegativeRange(1), Error);
  assertThrows(() => testFloat64NegativeRange(-2), Error);
  %OptimizeFunctionOnNextCall(testFloat64NegativeRange);
  assertThrows(() => testFloat64NegativeRange(-1), Error);
  assertUnoptimized(testFloat64NegativeRange);
}

// Test Float64 to Uint64 with a range containing both positive and negative
// integers. Valid positive values should use the fast path, while negative
// values must deopt and throw.
{
  function testFloat64MixedRange(v) {
    let val = (v | 0) + 0.1;
    val = Math.floor(val);
    val = Math.max(-10, Math.min(10, val));
    return fast_c_api.sum_uint64_as_number(val, 0);
  }

  %PrepareFunctionForOptimization(testFloat64MixedRange);
  testFloat64MixedRange(1);
  testFloat64MixedRange(2);
  %OptimizeFunctionOnNextCall(testFloat64MixedRange);
  assertEquals(3, testFloat64MixedRange(3));

  fast_c_api.reset_counts();
  assertEquals(4, testFloat64MixedRange(4));
  assertTrue(fast_c_api.fast_call_count() > 0);

  assertThrows(() => testFloat64MixedRange(-1), Error);
  assertUnoptimized(testFloat64MixedRange);
}
