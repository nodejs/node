// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboshaft-from-maglev --turbofan
// Flags: --no-always-turbofan

// TODO(dmercadier): re-allow optimization of these functions once the
// maglev-to-turboshaft graph builder supports everything they need.
%NeverOptimizeFunction(assertEquals);
%NeverOptimizeFunction(assertOptimized);
%NeverOptimizeFunction(assertUnoptimized);

function math_smi(x, y) {
  let a = x * y;
  a = a + 152;
  a = a / x;
  a = a - y;
  a = a % 5;
  return a;
}
%PrepareFunctionForOptimization(math_smi);
assertEquals(3, math_smi(4, 3));
%OptimizeFunctionOnNextCall(math_smi);
assertEquals(3, math_smi(4, 3));
assertOptimized(math_smi);
assertEquals(NaN, math_smi("a", "b"));
assertUnoptimized(math_smi);

function math_float(x, y) {
  let a = x * y;
  let b = a + 152.56;
  let c = b / x;
  let e = c - Math.round(y);
  let f = e % 5.56;
  let g = f ** x;
  let h = -g;
  return h;
}
%PrepareFunctionForOptimization(math_float);
assertEquals(-42.56563728706824, math_float(4.21, 3.56));
%OptimizeFunctionOnNextCall(math_float);
assertEquals(-42.56563728706824, math_float(4.21, 3.56));
assertOptimized(math_float);

function cmp(which, a, b) {
  if (which == 0) { return a > b; }
  if (which == 1) { return a >= b; }
  if (which == 2) { return a < b; }
  if (which == 3) { return a <= b; }
}
%PrepareFunctionForOptimization(cmp);
// >
assertEquals(false, cmp(0, 10, 20));
assertEquals(true, cmp(0, 20, 10));
assertEquals(false, cmp(0, 15, 15));
// >=
assertEquals(false, cmp(1, 10, 20));
assertEquals(true, cmp(1, 20, 10));
assertEquals(true, cmp(1, 15, 15));
// <
assertEquals(true, cmp(2, 10, 20));
assertEquals(false, cmp(2, 20, 10));
assertEquals(false, cmp(2, 15, 15));
// <=
assertEquals(true, cmp(3, 10, 20));
assertEquals(false, cmp(3, 20, 10));
assertEquals(true, cmp(3, 15, 15));

%OptimizeFunctionOnNextCall(cmp);
// >
assertEquals(false, cmp(0, 10, 20));
assertEquals(true, cmp(0, 20, 10));
assertEquals(false, cmp(0, 15, 15));
// >=
assertEquals(false, cmp(1, 10, 20));
assertEquals(true, cmp(1, 20, 10));
assertEquals(true, cmp(1, 15, 15));
// <
assertEquals(true, cmp(2, 10, 20));
assertEquals(false, cmp(2, 20, 10));
assertEquals(false, cmp(2, 15, 15));
// <=
assertEquals(true, cmp(3, 10, 20));
assertEquals(false, cmp(3, 20, 10));
assertEquals(true, cmp(3, 15, 15));
assertOptimized(cmp);

function bitwise_smi(a, b) {
  let x = a | b;
  x = x & 52358961;
  x = x ^ b;
  x = x >> 2;
  x = x << 5;
  x = x >>> 1;
  return ~x;
}
%PrepareFunctionForOptimization(bitwise_smi);
assertEquals(-23041, bitwise_smi(1548, 45235));
%OptimizeFunctionOnNextCall(bitwise_smi);
assertEquals(-23041, bitwise_smi(1548, 45235));
assertOptimized(bitwise_smi);

function simple_loop(x) {
  let s = 0;
  for (let i = 0; i < 4; i++) {
    s += i + x;
  }
  return s;
}
%PrepareFunctionForOptimization(simple_loop);
assertEquals(74, simple_loop(17));
%OptimizeFunctionOnNextCall(simple_loop);
assertEquals(74, simple_loop(17));
assertOptimized(simple_loop);

function load_smi_arr(arr, idx) {
  return arr[1] + arr[idx];
}
{
  let smi_arr = [1, 2, 3, 4, {}];
  %PrepareFunctionForOptimization(load_smi_arr);
  assertEquals(6, load_smi_arr(smi_arr, 3));
  %OptimizeFunctionOnNextCall(load_smi_arr);
  assertEquals(6, load_smi_arr(smi_arr, 3));
  assertOptimized(load_smi_arr);

  // String indices currently work without requiring deopt.
  assertEquals(5, load_smi_arr(smi_arr, '2'));
  assertOptimized(load_smi_arr);
}

function load_double_arr(arr, idx) {
  return arr[2] + arr[idx];
}
{
  let double_arr = [1.552, 2.425, 3.526, 4.596, 5.986, 6.321];
  %PrepareFunctionForOptimization(load_double_arr);
  assertEquals(8.122, load_double_arr(double_arr, 3));
  %OptimizeFunctionOnNextCall(load_double_arr);
  assertEquals(8.122, load_double_arr(double_arr, 3));
  assertOptimized(load_double_arr);

  // String indices currently work without requiring deopt.
  assertEquals(5.951, load_double_arr(double_arr, '1'));
  assertOptimized(load_double_arr);
}

// Simple JS function call
{
  %NeverOptimizeFunction(h);
  function h(x) { return x; }
  function g(x) { return h(x); }
  function f(x) { return g(x); }

  %PrepareFunctionForOptimization(g);
  %PrepareFunctionForOptimization(f);
  assertEquals(42, f(42));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(42, f(42));
  assertOptimized(f);
}

// Simple JS call with receiver
{
  function f(o) { return o.x(17); }

  let o = { y : 42, x : function(a) { return a + this.y; } };

  %PrepareFunctionForOptimization(f);
  assertEquals(59, f(o));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(59, f(o));
  assertOptimized(f);
}

// Lazy deopt during JS function call
{
  %NeverOptimizeFunction(h);
  function h(x, d) {
    if (d == 2) { return f(x, d-1); }
    if (d == 1) {
      // Calling `f` with a string as input will trigger an eager deopt of `f`,
      // which will also trigger a lazy deopt of all instances `f` on the caller
      // stack.
      return f("str", d-1);
    }
    return x;
  }

  function g(x, d) {
    let tmp = x * 12;
    let v = h(x, d);
    return tmp + v;
  }

  function f(x, d) {
    let a = x + 2;
    return g(a, d);
  }

  %PrepareFunctionForOptimization(f);
  %PrepareFunctionForOptimization(g);
  assertEquals(572, f(42, 0));

  %OptimizeFunctionOnNextCall(f);
  assertEquals(572, f(42, 0));
  assertOptimized(f);
  assertEquals("528552NaNstr2", f(42, 2));
  assertUnoptimized(f);
}

// Testing deopt with raw floats and raw integers in the frame state.
{
  %NeverOptimizeFunction(sum);
  function sum(...args) {
    return args.reduce((a,b) => a + b, 0);
  }

  function f(a, b, c) {
    let x = a * 4.25;
    let y = b * 17;
    // This call to `sum` causes `x` and `y` to be part of the frame state.
    let s = sum(a, b);
    let z = b + c;
    // This call is just to use the values we computed before.
    return sum(s, x, y, z);
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(113.39, f(2.36, 5, 6));

  %OptimizeFunctionOnNextCall(f);
  assertEquals(113.39, f(2.36, 5, 6));
  assertOptimized(f);
  assertEquals(113.93, f(2.36, 5, 6.54));
  assertUnoptimized(f);
}
