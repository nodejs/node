// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-fast-api --turbo-fast-api-calls
const fast_c_api = new d8.test.FastCAPI();

function sum_uint64_as_number(a, b) {
  return fast_c_api.sum_uint64_as_number(a, b);
}

function test(label) {
  fast_c_api.reset_counts();
  assertThrows(() => sum_uint64_as_number(-1, 0), Error);
}

// Non-optimized: slow path rejects -1
test("Interpreted");

// Optimize
%PrepareFunctionForOptimization(sum_uint64_as_number);
sum_uint64_as_number(1, 2);
sum_uint64_as_number(3, 4);
%OptimizeFunctionOnNextCall(sum_uint64_as_number);
sum_uint64_as_number(5, 6);

// Optimized: fast path accepts -1, reinterprets as uint64
test("Optimized ");
