// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

// Exact constant Int32 divisions (both Div and DivSmi).
(function testExactConstantDivide() {
  function div(a, b) {
    return a / b;
  }
  function f() {
    let n = 15908864;
    let min_int = 1 << 31;
    let d1 = n / 4;
    let d2 = div(100, -5);
    let d3 = div(-100, -5);
    let d4 = div(0, 7);
    let d5 = div(min_int, 2);
    return d1 + d2 + d3 + d4 + d5;
  }

  %PrepareFunctionForOptimization(div);
  %PrepareFunctionForOptimization(f);
  const expected = 3977216 + (-20) + 20 + 0 + (-1073741824);
  assertEquals(expected, f());
  assertEquals(expected, f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(expected, f());
})();

// Non-exact constant division must not fold to truncated Int32.
(function testNonExactConstantDivide() {
  function div(a, b, run) {
    if (!run) return 0;
    return a / b;
  }
  function f(run) {
    return div(7, 2, run);
  }

  %PrepareFunctionForOptimization(div);
  %PrepareFunctionForOptimization(f);
  assertEquals(4, div(8, 2, true));
  assertEquals(-3, div(-12, 4, true));
  assertEquals(0, f(false));
  assertEquals(0, f(false));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(0, f(false));
  assertEquals(3.5, f(true));
})();

// Division by zero must not fold or trigger divide-by-zero UB.
(function testDivideByZero() {
  function div(a, b, run) {
    if (!run) return 0;
    return a / b;
  }
  function f(run) {
    return div(42, 0, run);
  }

  %PrepareFunctionForOptimization(div);
  %PrepareFunctionForOptimization(f);
  assertEquals(4, div(8, 2, true));
  assertEquals(-3, div(-12, 4, true));
  assertEquals(0, f(false));
  assertEquals(0, f(false));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(0, f(false));
  assertEquals(Infinity, f(true));
})();

// kMinInt / -1 overflows Int32 and must not fold or trigger signed overflow UB.
(function testMinIntDivideMinusOne() {
  function div(a, b, run) {
    if (!run) return 0;
    return a / b;
  }
  function f(run) {
    let min_int = 1 << 31;
    return div(min_int, -1, run);
  }

  %PrepareFunctionForOptimization(div);
  %PrepareFunctionForOptimization(f);
  assertEquals(4, div(8, 2, true));
  assertEquals(-3, div(-12, 4, true));
  assertEquals(0, f(false));
  assertEquals(0, f(false));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(0, f(false));
  assertEquals(2147483648, f(true));
})();

// 0 / negative must produce -0, not fold to +0.
(function testMinusZeroDivide() {
  function div1(a, b, run) {
    if (!run) return 0;
    return a / b;
  }
  function div4(a, b, run) {
    if (!run) return 0;
    return a / b;
  }
  function f1(run) {
    return div1(0, -1, run);
  }
  function f4(run) {
    return div4(0, -4, run);
  }

  %PrepareFunctionForOptimization(div1);
  %PrepareFunctionForOptimization(div4);
  %PrepareFunctionForOptimization(f1);
  %PrepareFunctionForOptimization(f4);
  assertEquals(4, div1(8, 2, true));
  assertEquals(-3, div1(-12, 4, true));
  assertEquals(4, div4(8, 2, true));
  assertEquals(-3, div4(-12, 4, true));
  assertEquals(0, f1(false));
  assertEquals(0, f1(false));
  assertEquals(0, f4(false));
  assertEquals(0, f4(false));
  %OptimizeFunctionOnNextCall(f1);
  %OptimizeFunctionOnNextCall(f4);
  assertEquals(0, f1(false));
  assertEquals(0, f4(false));
  assertEquals(-Infinity, 1 / f1(true));
  assertEquals(-Infinity, 1 / f4(true));
  // Ensure feedback was updated by the -0 result and re-optimization does not
  // enter a deopt loop.
  %OptimizeFunctionOnNextCall(f1);
  %OptimizeFunctionOnNextCall(f4);
  assertEquals(-Infinity, 1 / f1(true));
  assertEquals(-Infinity, 1 / f4(true));
  assertOptimized(f1);
  assertOptimized(f4);
})();

// Loop bound derived from constant Int32 division enables range analysis.
(function testLoopBoundFromConstantDivide() {
  function sumQuarter(arr) {
    let bytes = 256;
    let limit = bytes / 4;
    let sum = 0;
    for (let i = 0; i < limit; i++) {
      sum = (sum + arr[i]) | 0;
    }
    return sum;
  }

  const arr = new Int32Array(64);
  for (let i = 0; i < 64; i++) arr[i] = i + 1;
  const expected = (64 * 65) / 2;

  %PrepareFunctionForOptimization(sumQuarter);
  assertEquals(expected, sumQuarter(arr));
  assertEquals(expected, sumQuarter(arr));
  %OptimizeFunctionOnNextCall(sumQuarter);
  assertEquals(expected, sumQuarter(arr));
})();
