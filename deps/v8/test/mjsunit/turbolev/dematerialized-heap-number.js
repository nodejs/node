// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function dematerialized_heap_number(n) {
  let f64 = 17.72;
  if (n < 0) {
    // We won't execute this code when collecting feedback, which means that
    // Maglev will do an unconditional deopt here (and {f64} will thus have no
    // use besides the frame state for this deopt).
    return f64 + 5;
  }
  return n + 2;
}

%PrepareFunctionForOptimization(dematerialized_heap_number);
assertEquals(7, dematerialized_heap_number(5));

%OptimizeFunctionOnNextCall(dematerialized_heap_number);
assertEquals(7, dematerialized_heap_number(5));
assertEquals(22.72, dematerialized_heap_number(-1));

function test_heapnumber_minus_zero(n) {
  let a = -0;
  if (n < 0) {
    n + 5;
    return a;
  }
  return n + 2;
}

%PrepareFunctionForOptimization(test_heapnumber_minus_zero);
assertEquals(7, test_heapnumber_minus_zero(5));
assertEquals(7, test_heapnumber_minus_zero(5));

%OptimizeFunctionOnNextCall(test_heapnumber_minus_zero);
assertEquals(7, test_heapnumber_minus_zero(5));
assertOptimized(test_heapnumber_minus_zero);
let result_minus_zero = test_heapnumber_minus_zero(-1);
assertUnoptimized(test_heapnumber_minus_zero);
assertEquals(-0, result_minus_zero);
assertEquals(1 / -0, -Infinity);
assertEquals(1 / result_minus_zero, -Infinity);

function test_heapnumber_nan(n) {
  let a = NaN;
  if (n < 0) {
    n + 5;
    return a;
  }
  return n + 2;
}

%PrepareFunctionForOptimization(test_heapnumber_nan);
assertEquals(7, test_heapnumber_nan(5));
assertEquals(7, test_heapnumber_nan(5));

%OptimizeFunctionOnNextCall(test_heapnumber_nan);
assertEquals(7, test_heapnumber_nan(5));
assertOptimized(test_heapnumber_nan);
let result_nan = test_heapnumber_nan(-1);
assertUnoptimized(test_heapnumber_nan);
assertTrue(isNaN(result_nan));

function test_heapnumber_infinity(n) {
  let a = Infinity;
  if (n < 0) {
    n + 5;
    return a;
  }
  return n + 2;
}

%PrepareFunctionForOptimization(test_heapnumber_infinity);
assertEquals(7, test_heapnumber_infinity(5));
assertEquals(7, test_heapnumber_infinity(5));

%OptimizeFunctionOnNextCall(test_heapnumber_infinity);
assertEquals(7, test_heapnumber_infinity(5));
assertOptimized(test_heapnumber_infinity);
let result_infinity = test_heapnumber_infinity(-1);
assertUnoptimized(test_heapnumber_infinity);
assertEquals(Infinity, result_infinity);

function test_heapnumber_minus_infinity(n) {
  let a = -Infinity;
  if (n < 0) {
    n + 5;
    return a;
  }
  return n + 2;
}

%PrepareFunctionForOptimization(test_heapnumber_minus_infinity);
assertEquals(7, test_heapnumber_minus_infinity(5));
assertEquals(7, test_heapnumber_minus_infinity(5));

%OptimizeFunctionOnNextCall(test_heapnumber_minus_infinity);
assertEquals(7, test_heapnumber_minus_infinity(5));
assertOptimized(test_heapnumber_minus_infinity);
let result_minus_infinity = test_heapnumber_minus_infinity(-1);
assertUnoptimized(test_heapnumber_minus_infinity);
assertEquals(-Infinity, result_minus_infinity);

function test_heapnumber_normal(n) {
  let a = 42.2;
  if (n < 0) {
    n + 5;
    return a;
  }
  return n + 2;
}

%PrepareFunctionForOptimization(test_heapnumber_normal);
assertEquals(7, test_heapnumber_normal(5));
assertEquals(7, test_heapnumber_normal(5));

%OptimizeFunctionOnNextCall(test_heapnumber_normal);
assertEquals(7, test_heapnumber_normal(5));
assertOptimized(test_heapnumber_normal);
let result_normal = test_heapnumber_normal(-1);
assertUnoptimized(test_heapnumber_normal);
assertEquals(42.2, result_normal);
