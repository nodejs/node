// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboshaft-from-maglev --turbofan
// Flags: --no-always-turbofan --no-stress-concurrent-inlining
// Flags: --invocation-count-for-turbofan=3000 --concurrent-recompilation
// Flags: --harmony-struct

// TODO(dmercadier): re-allow optimization of these functions once the
// maglev-to-turboshaft graph builder supports everything they need.
%NeverOptimizeFunction(assertEquals);
%NeverOptimizeFunction(assertOptimized);
%NeverOptimizeFunction(assertUnoptimized);

function math_smi(x, y, z) {
  let a = x * y;
  a = a + 152;
  a = a / x;
  a++;
  a = a - y;
  a = a % 5;
  a--;
  a = -z + a;
  return a;
}
%PrepareFunctionForOptimization(math_smi);
assertEquals(8, math_smi(4, 3, -5));
assertEquals(8, math_smi(4, 3, -5));
%OptimizeFunctionOnNextCall(math_smi);
assertEquals(8, math_smi(4, 3, -5));
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
assertEquals(-42.56563728706824, math_float(4.21, 3.56));
%OptimizeFunctionOnNextCall(math_float);
assertEquals(-42.56563728706824, math_float(4.21, 3.56));
assertOptimized(math_float);

// Comparisons
{
  function cmp_int32(which, a, b) {
    if (which == 0) { return a > b; }
    if (which == 1) { return a >= b; }
    if (which == 2) { return a < b; }
    if (which == 3) { return a <= b; }
  }
  %PrepareFunctionForOptimization(cmp_int32);
  // >
  assertEquals(false, cmp_int32(0, 10, 20));
  assertEquals(true, cmp_int32(0, 20, 10));
  assertEquals(false, cmp_int32(0, 15, 15));
  // >=
  assertEquals(false, cmp_int32(1, 10, 20));
  assertEquals(true, cmp_int32(1, 20, 10));
  assertEquals(true, cmp_int32(1, 15, 15));
  // <
  assertEquals(true, cmp_int32(2, 10, 20));
  assertEquals(false, cmp_int32(2, 20, 10));
  assertEquals(false, cmp_int32(2, 15, 15));
  // <=
  assertEquals(true, cmp_int32(3, 10, 20));
  assertEquals(false, cmp_int32(3, 20, 10));
  assertEquals(true, cmp_int32(3, 15, 15));

  %OptimizeFunctionOnNextCall(cmp_int32);
  // >
  assertEquals(false, cmp_int32(0, 10, 20));
  assertEquals(true, cmp_int32(0, 20, 10));
  assertEquals(false, cmp_int32(0, 15, 15));
  // >=
  assertEquals(false, cmp_int32(1, 10, 20));
  assertEquals(true, cmp_int32(1, 20, 10));
  assertEquals(true, cmp_int32(1, 15, 15));
  // <
  assertEquals(true, cmp_int32(2, 10, 20));
  assertEquals(false, cmp_int32(2, 20, 10));
  assertEquals(false, cmp_int32(2, 15, 15));
  // <=
  assertEquals(true, cmp_int32(3, 10, 20));
  assertEquals(false, cmp_int32(3, 20, 10));
  assertEquals(true, cmp_int32(3, 15, 15));
  assertOptimized(cmp_int32);

  function cmp_float64(which, a, b) {
    if (which == 0) { return a > b; }
    if (which == 1) { return a >= b; }
    if (which == 2) { return a < b; }
    if (which == 3) { return a <= b; }
  }
  %PrepareFunctionForOptimization(cmp_float64);
  // >
  assertEquals(false, cmp_float64(0, 10.25, 20.25));
  assertEquals(true, cmp_float64(0, 20.25, 10.25));
  assertEquals(false, cmp_float64(0, 15.25, 15.25));
  // >=
  assertEquals(false, cmp_float64(1, 10.25, 20.25));
  assertEquals(true, cmp_float64(1, 20.25, 10.25));
  assertEquals(true, cmp_float64(1, 15.25, 15.25));
  // <
  assertEquals(true, cmp_float64(2, 10.25, 20.25));
  assertEquals(false, cmp_float64(2, 20.25, 10.25));
  assertEquals(false, cmp_float64(2, 15.25, 15.25));
  // <=
  assertEquals(true, cmp_float64(3, 10.25, 20.25));
  assertEquals(false, cmp_float64(3, 20.25, 10.25));
  assertEquals(true, cmp_float64(3, 15.25, 15.25));

  %OptimizeFunctionOnNextCall(cmp_float64);
  // >
  assertEquals(false, cmp_float64(0, 10.25, 20.25));
  assertEquals(true, cmp_float64(0, 20.25, 10.25));
  assertEquals(false, cmp_float64(0, 15.25, 15.25));
  // >=
  assertEquals(false, cmp_float64(1, 10.25, 20.25));
  assertEquals(true, cmp_float64(1, 20.25, 10.25));
  assertEquals(true, cmp_float64(1, 15.25, 15.25));
  // <
  assertEquals(true, cmp_float64(2, 10.25, 20.25));
  assertEquals(false, cmp_float64(2, 20.25, 10.25));
  assertEquals(false, cmp_float64(2, 15.25, 15.25));
  // <=
  assertEquals(true, cmp_float64(3, 10.25, 20.25));
  assertEquals(false, cmp_float64(3, 20.25, 10.25));
  assertEquals(true, cmp_float64(3, 15.25, 15.25));
  assertOptimized(cmp_float64);

  // Number equality should deopt when passed an Oddball (because undefined's
  // value is NaN, which leads to undefined != undefined without the deopt).
  function equal_num(a, b) { return a == b; }

  %PrepareFunctionForOptimization(equal_num);
  assertEquals(true, equal_num(.5, .5));
  %OptimizeFunctionOnNextCall(equal_num);
  assertEquals(true, equal_num(.5, .5));
  assertOptimized(equal_num);
  assertEquals(true, equal_num(undefined, undefined));
  assertUnoptimized(equal_num);
}

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
assertEquals(74, simple_loop(17));
%OptimizeFunctionOnNextCall(simple_loop);
assertEquals(74, simple_loop(17));
assertOptimized(simple_loop);

// Testing field loads
{
  function load_field(o) {
    let x = o.x;
    let y = o.y;
    return x + y;
  }

  let o = { x : 42, y : 15.71 };

  %PrepareFunctionForOptimization(load_field);
  assertEquals(57.71, load_field(o));
  %OptimizeFunctionOnNextCall(load_field);
  assertEquals(57.71, load_field(o));
  assertOptimized(load_field);
}

// Testing array loads
{
  function load_smi_arr(arr, idx) {
    return arr[1] + arr[idx];
  }
  let smi_arr = [1, 2, 3, 4, {}];
  %PrepareFunctionForOptimization(load_smi_arr);
  assertEquals(6, load_smi_arr(smi_arr, 3));
  assertEquals(6, load_smi_arr(smi_arr, 3));
  %OptimizeFunctionOnNextCall(load_smi_arr);
  assertEquals(6, load_smi_arr(smi_arr, 3));
  assertOptimized(load_smi_arr);

  // String indices currently work without requiring deopt.
  assertEquals(5, load_smi_arr(smi_arr, '2'));
  assertOptimized(load_smi_arr);

  function load_double_arr(arr, idx) {
    return arr[2] + arr[idx];
  }
  let double_arr = [1.552, 2.425, 3.526, 4.596, 5.986, 6.321];
  %PrepareFunctionForOptimization(load_double_arr);
  assertEquals(8.122, load_double_arr(double_arr, 3));
  assertEquals(8.122, load_double_arr(double_arr, 3));
  %OptimizeFunctionOnNextCall(load_double_arr);
  assertEquals(8.122, load_double_arr(double_arr, 3));
  assertOptimized(load_double_arr);

  // String indices currently work without requiring deopt.
  assertEquals(5.951, load_double_arr(double_arr, '1'));
  assertOptimized(load_double_arr);

  function load_holey_fixed_double(arr, idx) {
    return arr[idx];
  }
  let holey_double_arr = [2.58,3.41,,4.55];

  %PrepareFunctionForOptimization(load_holey_fixed_double);
  assertEquals(3.41, load_holey_fixed_double(holey_double_arr, 1));
  %OptimizeFunctionOnNextCall(load_holey_fixed_double);
  assertEquals(3.41, load_holey_fixed_double(holey_double_arr, 1));
  assertOptimized(load_holey_fixed_double);
  // Loading a hole should trigger a deopt
  assertEquals(undefined, load_holey_fixed_double(holey_double_arr, 2));
  assertUnoptimized(load_holey_fixed_double);

  // Reoptimizing, holes should now be handled
  %OptimizeMaglevOnNextCall(load_holey_fixed_double);
  assertEquals(3.41, load_holey_fixed_double(holey_double_arr, 1));
  %OptimizeFunctionOnNextCall(load_holey_fixed_double);
  assertEquals(3.41, load_holey_fixed_double(holey_double_arr, 1));
  assertEquals(undefined, load_holey_fixed_double(holey_double_arr, 2));
  assertOptimized(load_holey_fixed_double);

  function load_hole(arr, idx) {
    return arr[idx];
  }
  let holey_arr = [ {}, 3.41, /* hole */, 4.55 ];

  %PrepareFunctionForOptimization(load_hole);
  assertEquals(undefined, load_hole(holey_arr, 2));
  %OptimizeFunctionOnNextCall(load_hole);
  assertEquals(undefined, load_hole(holey_arr, 2));
  assertOptimized(load_hole);
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
  assertEquals(42, f(42));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(42, f(42));
  assertOptimized(f);
}

// Simple JS call with receiver
{
  function f(o) { return o.x(17); }

  let o = { y : 42, x : function(a) { return a + this.y; } };
  %NeverOptimizeFunction(o.x);

  %PrepareFunctionForOptimization(f);
  assertEquals(59, f(o));
  assertEquals(59, f(o));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(59, f(o));
  assertOptimized(f);
}

// Unconditional deopt
{
  function deopt(x) {
    if (x) { return 42; }
    else {
      // We won't gather feedback for this during feedback collection, so
      // Maglev will generate an unconditional deopt for this branch.
      return x + 17;
    }
  }

  %PrepareFunctionForOptimization(deopt);
  assertEquals(42, deopt(1));
  %OptimizeFunctionOnNextCall(deopt);
  assertEquals(42, deopt(1));
  assertOptimized(deopt);
  assertEquals(17, deopt(0));
  assertUnoptimized(deopt);
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
  assertEquals(113.39, f(2.36, 5, 6));

  %OptimizeFunctionOnNextCall(f);
  assertEquals(113.39, f(2.36, 5, 6));
  assertOptimized(f);
  assertEquals(113.93, f(2.36, 5, 6.54));
  assertUnoptimized(f);
}

// Testing exceptions (but ignoring the exception value).
{
  function h(x) {
    if (x) { willThrow(); }
    else { return 17; }
  }
  %NeverOptimizeFunction(h);

  function f(a, b) {
    let r = a;
    try {
      r = h(a);
      return h(b) + r;
    } catch {
      return r * b;
    }
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(34, f(0, 0)); // Won't cause an exception
  assertEquals(187, f(0, 11)); // Will cause an exception on the 2nd call to h
  assertEquals(0, f(7, 0)); // Will cause an exception on the 1st call to h
  %OptimizeFunctionOnNextCall(f);
  assertEquals(34, f(0, 0));
  assertEquals(187, f(0, 11));
  assertEquals(0, f(7, 0));
  assertOptimized(f);
}

// Testing exceptions (single throwing point, using the exception value).
{
  function h(x) {
    if (x) { willThrow(); }
    else { return 17; }
  }
  %NeverOptimizeFunction(h);

  function exc_f(a) {
    try {
      return h(a);
    } catch(e) {
      // Stringifying the exception for easier comparison.
      return "abc" + e;
    }
  }

  %PrepareFunctionForOptimization(exc_f);
  assertEquals(17, exc_f(0));
  let err = exc_f(1); // Will cause an exception.
  %OptimizeFunctionOnNextCall(exc_f);
  assertEquals(17, exc_f(0));
  assertEquals(err, exc_f(1));
  assertOptimized(exc_f);
}

// Testing exceptions (multiple throwing points, using the exception value).
{
  function h(x) {
    if (x) { willThrow(); }
    else { return 17; }
  }
  %NeverOptimizeFunction(h);

  function multi_exc_f(a, b) {
    let r = a;
    try {
      r = h(a);
      return h(b) + r;
    }
    catch(e) {
      // Stringifying the exception for easier comparison.
      return "abc" + e + r;
    }
  }

  %PrepareFunctionForOptimization(multi_exc_f);
  assertEquals(34, multi_exc_f(0, 0)); // Won't cause an exception.
  let err1 = multi_exc_f(0, 11); // Will cause an exception on the 2nd call to h.
  let err2 = multi_exc_f(7, 0); // Will cause an exception on the 1st call to h.
  %OptimizeFunctionOnNextCall(multi_exc_f);
  assertEquals(34, multi_exc_f(0, 0));
  assertEquals(err1, multi_exc_f(0, 11));
  assertEquals(err2, multi_exc_f(7, 0));
  assertOptimized(multi_exc_f);
}

// Testing builtin calls
{
  // String comparison (which are currently done with builtin calls in Maglev).
  function cmp_str(a, b) {
    return a < b;
  }

  %PrepareFunctionForOptimization(cmp_str);
  assertEquals(true, cmp_str("abc", "def"));
  %OptimizeFunctionOnNextCall(cmp_str);
  assertEquals(true, cmp_str("abc", "def"));
  assertOptimized(cmp_str);

  // Megamorphic load.
  function load(o) {
    return o.x;
  }

  let o1 = { x : 42 };
  let o2 = { a : {}, x : 2.5 };
  let o3 = 42;
  let o4 = { b : 42, c: {}, x : 5.35 };
  let o5 = { u : 14, c : 2.28, d: 4.2, x : 5 };

  %PrepareFunctionForOptimization(load);
  assertEquals(42, load(o1));
  assertEquals(2.5, load(o2));
  assertEquals(undefined, load(o3));
  assertEquals(5.35, load(o4));
  assertEquals(5, load(o5));
  %OptimizeFunctionOnNextCall(load);
  assertEquals(42, load(o1));
  assertEquals(2.5, load(o2));
  assertEquals(undefined, load(o3));
  assertEquals(5.35, load(o4));
  assertEquals(5, load(o5));
  assertOptimized(load);

  // charAt is a builtin call but is done with a CallKnowJSFunctionCall
  function string_char_at(s) {
    return s.charAt(2);
  }

  %PrepareFunctionForOptimization(string_char_at);
  assertEquals("c", string_char_at("abcdef"));
  %OptimizeFunctionOnNextCall(string_char_at);
  assertEquals("c", string_char_at("abcdef"));
  assertOptimized(string_char_at);
}

// Testing stores
{
  function store_field(o, v) {
    o.x = 17; // Tagged field, no write barrier
    o.y = v; // Tagged field, with write barrier
    o.z = 12.29; // Double field
    return o;
  }

  let o = { x : 42, y : 10, z : 14.58 };

  %PrepareFunctionForOptimization(store_field);
  assertEquals({ x : 17, y : undefined, z : 12.29 }, store_field(o));
  o = { x : 42, y : 10, z : 14.58 }; // Resetting {o}
  %OptimizeFunctionOnNextCall(store_field);
  assertEquals({ x : 17, y : undefined, z : 12.29 }, store_field(o));
  assertOptimized(store_field);

  function store_arr(obj_arr, double_arr) {
    obj_arr[0] = 42; // FixedArray, no write barrier
    obj_arr[1] = double_arr; // FixedArray, with write barrier
    double_arr[1] = 42.25; // DoubleFixedArray
  }

  let obj_arr = [0, {}, 2];
  let double_arr = [1.56, 2.68, 3.51];

  %PrepareFunctionForOptimization(store_arr);
  store_arr(obj_arr, double_arr);
  assertEquals([42, double_arr, 2], obj_arr);
  assertEquals([1.56, 42.25, 3.51], double_arr);

  // Resetting {obj_arr} and {double_arr}
  obj_arr[0] = 0;
  obj_arr[1] = {};
  double_arr[1] = 2.68;

  %OptimizeFunctionOnNextCall(store_arr);
  store_arr(obj_arr, double_arr);
  assertEquals([42, double_arr, 2], obj_arr);
  assertEquals([1.56, 42.25, 3.51], double_arr);
  assertOptimized(store_arr);
}

// Testing branches
{
  function branch_cmp(w32, f64) {
    let c1 = w32 + 2;
    let v;
    if (c1) {
      v = c1 + 12;
    } else {
      v = c1 + 17;
    }
    let c2 = f64 + 1.5;
    if (c2) {
      v += 10;
    } else {
      v += 3;
    }
    if (f64 > 1.5) {
      v += 2;
    } else {
      v += 7;
    }
    return v;
  }

  %PrepareFunctionForOptimization(branch_cmp);
  assertEquals(41, branch_cmp(15, 3.25));
  assertEquals(29, branch_cmp(-2, 3.25));
  assertEquals(27, branch_cmp(-2, -1.5));
  %OptimizeFunctionOnNextCall(branch_cmp);
  assertEquals(41, branch_cmp(15, 3.25));
  assertEquals(29, branch_cmp(-2, 3.25));
  assertEquals(27, branch_cmp(-2, -1.5));
  assertOptimized(branch_cmp);
}

// Test BranchIfReferenceEqual and TaggedEqual
{
  function f(x, y) {
    let is_eq = x === y;
    let r = is_eq + 2;
    if (x === y) {
      return r + 42;
    } else {
      return r + 25;
    }
  }

  let o = {};

  %PrepareFunctionForOptimization(f);
  assertEquals(27, f(o, []));
  assertEquals(45, f(o, o));
  %OptimizeMaglevOnNextCall(f);
  assertEquals(27, f(o, []));
  assertEquals(45, f(o, o));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(27, f(o, []));
  assertEquals(45, f(o, o));
  assertOptimized(f);

  // Making sure that strings trigger a deopt (since === for strings is regular
  // string equality, not reference equality).
  assertEquals(45, f("abcdefghijklmno", "abcde" + "fghijklmno"));
  assertUnoptimized(f);
}

// Testing allocation of objects.
{
  function alloc() {
    return [1, {x : 42.27}, 3, "abc"];
  }

  %PrepareFunctionForOptimization(alloc);
  assertEquals([1, {x : 42.27}, 3, "abc"], alloc());
  %OptimizeMaglevOnNextCall(alloc);
  assertEquals([1, {x : 42.27}, 3, "abc"], alloc());
  %OptimizeFunctionOnNextCall(alloc);
  assertEquals([1, {x : 42.27}, 3, "abc"], alloc());
  assertOptimized(alloc);
}

// Testing string opcodes
{
  function f(short_str, long_str) {
    let cons_str = short_str + long_str;
    let seq_str = short_str + "cd";
    if (seq_str == "abcdcd") {
      return cons_str.length;
    } else {
      return seq_str[2];
    }
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(19, f("abcd", "abcdefghijklmno"));
  assertEquals("c", f("abcde", "abcdefghijklmno"));
  %OptimizeMaglevOnNextCall(f);
  assertEquals(19, f("abcd", "abcdefghijklmno"));
  assertEquals("c", f("abcde", "abcdefghijklmno"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(19, f("abcd", "abcdefghijklmno"));
  assertEquals("c", f("abcde", "abcdefghijklmno"));
  assertOptimized(f);

  function string_cmp(str1, str2, cmp) {
    if (cmp == 0) { return str1 < str2; }
    if (cmp == 1) { return str1 <= str2; }
    if (cmp == 2) { return str1 > str2; }
    if (cmp == 3) { return str1 >= str2; }
    if (cmp == 4) { return str1 == str2; }
    if (cmp == 5) { return str1 != str2; }
  }

  %PrepareFunctionForOptimization(string_cmp);
  assertEquals(true, string_cmp("ab", "cd", 0)); // <
  assertEquals(false, string_cmp("ab", "ab", 0));
  assertEquals(false, string_cmp("ar", "ab", 0));
  assertEquals(true, string_cmp("ab", "cd", 1)); // <=
  assertEquals(true, string_cmp("ab", "ab", 1));
  assertEquals(false, string_cmp("ar", "ab", 1));
  assertEquals(true, string_cmp("cd", "ab", 2)); // >
  assertEquals(false, string_cmp("ab", "ab", 2));
  assertEquals(false, string_cmp("ab", "cd", 2));
  assertEquals(true, string_cmp("cd", "ab", 3)); // >=
  assertEquals(true, string_cmp("ab", "ab", 3));
  assertEquals(false, string_cmp("ab", "cd", 3));
  assertEquals(true, string_cmp("ab", "ab", 4)); // ==
  assertEquals(false, string_cmp("ar", "ab", 4));
  assertEquals(true, string_cmp("ab", "cd", 5)); // !=
  assertEquals(false, string_cmp("ab", "ab", 5));

  %OptimizeFunctionOnNextCall(string_cmp);
  assertEquals(true, string_cmp("ab", "cd", 0)); // <
  assertEquals(false, string_cmp("ab", "ab", 0));
  assertEquals(false, string_cmp("ar", "ab", 0));
  assertOptimized(string_cmp);
  assertEquals(true, string_cmp("ab", "cd", 1)); // <=
  assertEquals(true, string_cmp("ab", "ab", 1));
  assertEquals(false, string_cmp("ar", "ab", 1));
  assertOptimized(string_cmp);
  assertEquals(true, string_cmp("cd", "ab", 2)); // >
  assertEquals(false, string_cmp("ab", "ab", 2));
  assertEquals(false, string_cmp("ab", "cd", 2));
  assertOptimized(string_cmp);
  assertEquals(true, string_cmp("cd", "ab", 3)); // >=
  assertEquals(true, string_cmp("ab", "ab", 3));
  assertEquals(false, string_cmp("ab", "cd", 3));
  assertOptimized(string_cmp);
  assertEquals(true, string_cmp("ab", "ab", 4)); // ==
  assertEquals(false, string_cmp("ar", "ab", 4));
  assertEquals(true, string_cmp("ab", "cd", 5)); // !=
  assertEquals(false, string_cmp("ab", "ab", 5));
  assertOptimized(string_cmp);

  // Passing a non-internal string as a parameter will trigger a deopt in "=="
  let str = "abcdefghi";
  assertEquals(false, string_cmp(str + "azeazeaze", "abc", 4));
  assertUnoptimized(string_cmp);

  function string_char_code(s, c) {
    return String.fromCharCode(73) + s.charCodeAt(1) + s.codePointAt(1);
  }

  %PrepareFunctionForOptimization(string_char_code);
  assertEquals("I5530474565", string_char_code("a\u{12345}c", 1));
  %OptimizeFunctionOnNextCall(string_char_code);
  assertEquals("I5530474565", string_char_code("a\u{12345}c", 1));
  assertOptimized(string_char_code);
}

// Testing generic builtins
{
  function f(a, b, c, d) {
    let x1 = a + b;
    let x2 = a - b;
    let x3 = a * b;
    let x4 = a / b;
    let x5 = a % b;
    let x6 = a ** b;
    let x7 = a & b;
    let x8 = a | b;
    let x9 = a ^ b;
    let x10 = a << b;
    let x11 = a >> b;
    let x12 = a >>> b;
    let x13 = ~a;
    let x14 = -a;
    let x15 = a < b;
    let x16 = a <= b;
    let x17 = a > b;
    let x18 = a >= b;
    let x19 = a == b;
    let x20 = a === b;
    // It's important to do the increment/decrement last, because they lead to
    // having numeric alternatives for {a} and {b} in Maglev, which leads Maglev
    // to compiling subsequent <, <=, >, and >= to Float64 comparisons despite
    // their generic feedback.
    let x21 = a++;
    let x22 = b--;
    let x23 = ++c;
    let x24 = --d;

    return [x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
            x16, x17, x18, x19, x20, x21, x22, x23, x24];
  }

  %PrepareFunctionForOptimization(f);
  let expected_1 = f(1, 2, 1, 2);
  let expected_2 = f("1", 2, "1", 2);
  let expected_3 = f(2, "1", 2, "1");
  let expected_4 = f([], 1, [], 1);
  let expected_5 = f({}, 1, {}, 1);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(expected_1, f(1, 2, 1, 2));
  assertEquals(expected_2, f("1", 2, "1", 2));
  assertEquals(expected_3, f(2, "1", 2, "1"));
  assertEquals(expected_4, f([], 1, [], 1));
  assertEquals(expected_5, f({}, 1, {}, 1));
  assertOptimized(f);
}

// Testing ToBoolean and LogicalNot on various datatypes.
{
  function f(x, y) {
    let a = x * 2;
    let b = a * 2.27;
    return [!a, !y, !b];
  }

  %PrepareFunctionForOptimization(f);
  assertEquals([false, false, false], f(4, 3));
  assertEquals([true, true, true], f(0, 0));
  %OptimizeFunctionOnNextCall(f);
  assertEquals([false, false, false], f(4, 3));
  assertEquals([true, true, true], f(0, 0));
  assertOptimized(f);
}

// Testing Float64 Ieee754 unary functions.
{
  function float_f(x) {
    let x1 = Math.abs(x);
    let x2 = Math.acos(x);
    let x3 = Math.acosh(x);
    let x4 = Math.asin(x);
    let x5 = Math.asinh(x);
    let x6 = Math.atan(x);
    let x7 = Math.atanh(x);
    let x8 = Math.cbrt(x);
    let x9 = Math.cos(x);
    let x10 = Math.cosh(x);
    let x11 = Math.exp(x);
    let x12 = Math.expm1(x);
    let x13 = Math.log(x);
    let x14 = Math.log1p(x);
    let x15 = Math.log10(x);
    let x16 = Math.log2(x);
    let x17 = Math.sin(x);
    let x18 = Math.sinh(x);
    let x19 = Math.tan(x);
    let x20 = Math.tanh(x);

    return [x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11,
            x12, x13, x14, x15, x16, x17, x18, x19, x20];
  }

  %PrepareFunctionForOptimization(float_f);
  let expected_1 = float_f(0.758);
  let expected_2 = float_f(2);

  %OptimizeFunctionOnNextCall(float_f);
  assertEquals(expected_1, float_f(0.758));
  assertEquals(expected_2, float_f(2));
  assertOptimized(float_f);
}

// Testing for-in loops.
{
  function forin(o) {
    let s = 0;
    for (let i in o) {
      s += o[i];
    }
    return s;
  }

  let o = { x : 42, y : 19, z: 5 };

  %PrepareFunctionForOptimization(forin);
  assertEquals(66, forin(o));
  %OptimizeFunctionOnNextCall(forin);
  assertEquals(66, forin(o));
  assertOptimized(forin);
}

// Testing loops with multiple forward edges.
{
  function g() { }
  %NeverOptimizeFunction(g);

  function multi_pred_loop(x) {
    let i = 0;
    if (x == 1) {
      g();
    } else {
      g();
    }
    while (i < 5) {
      i++;
      // Putting a call in the loop so that it cannot be peeled (because it's
      // not "trivial" anymore).
      g();
    }
    return i;
  }

  %PrepareFunctionForOptimization(multi_pred_loop);
  assertEquals(5, multi_pred_loop(1));
  assertEquals(5, multi_pred_loop(2));
  %OptimizeFunctionOnNextCall(multi_pred_loop);
  assertEquals(5, multi_pred_loop(1));
  assertEquals(5, multi_pred_loop(2));
  assertOptimized(multi_pred_loop);
}

// Testing reference error if hole
{
  function ref_err_if_hole(x) {
    switch (x) {
      case 0:
        let v = 17;
      case 1:
        return v;
    }
  }

  %PrepareFunctionForOptimization(ref_err_if_hole);
  assertEquals(17, ref_err_if_hole(0));
  %OptimizeFunctionOnNextCall(ref_err_if_hole);
  assertEquals(17, ref_err_if_hole(0));

  assertThrows(() => ref_err_if_hole(1), ReferenceError,
               "Cannot access 'v' before initialization");
  assertOptimized(ref_err_if_hole);
}

// Testing `eval()`, which tests ArgumentsLength, ArgumentsElements,
// CreateFunctionContext and CallRuntime.
{
  function f_eval() {
    let i = 0.1;
    eval();
    if (i) {
      const c = {};
      eval();
    }
  }

  %PrepareFunctionForOptimization(f_eval);
  f_eval();
  f_eval();
  %OptimizeFunctionOnNextCall(f_eval);
  f_eval();
  assertOptimized(f_eval);
}

// Testing typed arrays creations, loads and stores.
{
  function typed_arr() {
    const u16_arr = new Uint16Array(10);
    u16_arr[5] = 18;
    const i8_arr = new Int8Array(13);
    i8_arr[2] = 47;
    const f32_arr = new Float32Array(15);
    f32_arr[10] = 3.362;
    let u16 = u16_arr[1];
    let i8 = i8_arr[4];
    let f32 = f32_arr[9];
    return [u16_arr, i8_arr, f32_arr, u16, i8, f32];
  }

  %PrepareFunctionForOptimization(typed_arr);
  let a1 = typed_arr();
  %OptimizeFunctionOnNextCall(typed_arr);
  let a2 = typed_arr();
  assertEquals(a1, a2);
  assertOptimized(typed_arr);
}

// Testing clamped Uint8 typed array.
{
  let uint32_arr = new Uint32Array(5);
  uint32_arr[1] = 168;
  uint32_arr[2] = 5896;
  let uint8_clamped_arr = new Uint8ClampedArray(10);

  function store_clamped_array(x, small_num, large_num, nan) {
    // Storing Float64 (< 255)
    let f64_small = x + 3.45;
    uint8_clamped_arr[0] = f64_small;
    // Storing Float64 (> 255);
    let f64_large = x + 18956.586;
    uint8_clamped_arr[1] = f64_large;

    // Storing Int32 (< 255)
    let i32_small = x + 17;
    uint8_clamped_arr[2] = i32_small;
    // Storing Int32 (> 255)
    let i32_large = x + 25896;
    uint8_clamped_arr[3] = i32_large;

    // Storing uint32 (< 255)
    let uint32_small = uint32_arr[1];
    uint8_clamped_arr[4] = uint32_small;
    let uint32_large = uint32_arr[2];
    uint8_clamped_arr[5] = uint32_large;

    // Storing number (small)
    uint8_clamped_arr[6] = small_num;
    // Storing number (large)
    uint8_clamped_arr[7] = large_num;
    // Storing number (NaN)
    uint8_clamped_arr[8] = nan;
  }

  let expected = new Uint8ClampedArray([7,255,21,255,168,255,6,255,0,0]);

  %PrepareFunctionForOptimization(store_clamped_array);
  store_clamped_array(4, 6, 1896.365, NaN);
  assertEquals(expected, uint8_clamped_arr);

  // Reseting the array
  uint8_clamped_arr = new Uint8ClampedArray(10);

  %OptimizeFunctionOnNextCall(store_clamped_array);
  store_clamped_array(4, 6, 1896.365, NaN);
  assertEquals(expected, uint8_clamped_arr);
  assertOptimized(store_clamped_array);

  // Triggering deopt when trying to store a non-number
  store_clamped_array(4, "abc", 1896.365);
  expected[6] = 0;
  assertEquals(expected, uint8_clamped_arr);
  // Note: we don't assertUnoptimized here because in some configurations where
  // some clamped Uint8 operations are not supported, the stores of
  // `store_clamped_array` are compiled to SetKeyedGeneric by Maglev, and thus
  // do not deopt for non-numbers. If the result is correct, then deopt or not,
  // the correct thing probably happened.
}

// Testing dataview creation, loads and stores.
{
  function dataview() {
    const buffer = new ArrayBuffer(40);
    const dw = new DataView(buffer);
    dw.setInt8(4, 32);
    dw.setInt16(2, 152515);
    dw.setFloat64(8, 12.2536);
    return [dw.getInt16(0), dw.getInt8(4), dw.getFloat64(2), dw.getInt32(7)];
  }

  %PrepareFunctionForOptimization(dataview);
  let a1 = dataview();
  %OptimizeFunctionOnNextCall(dataview);
  let a2 = dataview();
  assertEquals(a1, a2);
  assertOptimized(dataview);
}

// Testing untagged phis.
{
  function fact(n) {
    let s = 1;
    while (n > 1) {
      s *= n;
      n--;
    }
    return s;
  }

  %PrepareFunctionForOptimization(fact);
  let n1 = fact(42);
  %OptimizeFunctionOnNextCall(fact);
  assertEquals(n1, fact(42));
  assertOptimized(fact);
}

// Testing a few array operations.
{
  let a = [1, 2, 3, 4];
  let b = [5, 6, 7, 8];

  function make_fast_arr(a, b) {
    let s = [0, 0, 0, 0];
    for (let i = 0; i < 4; i++) {
      s[i] = a[i] + b[i];
    }
    return s;
  }

  %PrepareFunctionForOptimization(make_fast_arr);
  assertEquals([6, 8, 10, 12], make_fast_arr(a, b));
  %OptimizeFunctionOnNextCall(make_fast_arr);
  assertEquals([6, 8, 10, 12], make_fast_arr(a, b));
  assertOptimized(make_fast_arr);

  function arr_oob_load(a, b) {
    let s = [0, 0, 0, 0, 0, 0];
    for (let i = 0; i < 6; i++) {
      // This will load out of bounds in {a} and {b}, which is fine.
      s[i] = a[i] + b[i];
    }
    return s;
  }

  %PrepareFunctionForOptimization(arr_oob_load);
  assertEquals([6, 8, 10, 12, NaN, NaN], arr_oob_load(a, b));
  %OptimizeFunctionOnNextCall(arr_oob_load);
  assertEquals([6, 8, 10, 12, NaN, NaN], arr_oob_load(a, b));
  assertOptimized(arr_oob_load);

  function ret_from_holey_arr(i) {
    let arr = new Array(10);
    arr[0] = 42;
    return arr[i];
  }

  %PrepareFunctionForOptimization(ret_from_holey_arr);
  assertEquals(42, ret_from_holey_arr(0));
  %OptimizeFunctionOnNextCall(ret_from_holey_arr);
  assertEquals(42, ret_from_holey_arr(0));
  assertOptimized(ret_from_holey_arr);

  // Triggering deopt by trying to return a hole.
  assertEquals(undefined, ret_from_holey_arr(1));
  assertUnoptimized(ret_from_holey_arr);

  // Reopting
  %OptimizeFunctionOnNextCall(ret_from_holey_arr);
  assertEquals(undefined, ret_from_holey_arr(1));
  // This time the hole is converted to undefined without deopting.
  assertOptimized(ret_from_holey_arr);
}

// Testing SetKeyedGeneric and GetKeyedGeneric.
{
  function generic_key(arr, i, j) {
    arr[i] = 45;
    return arr[j];
  }

  let arr = new Int32Array(42);

  %PrepareFunctionForOptimization(generic_key);
  assertEquals(undefined, generic_key(arr, -123456, -45896));
  %OptimizeFunctionOnNextCall(generic_key);
  assertEquals(undefined, generic_key(arr, -123456, -45896));
}

// Testing OSR.
{
  function g(x) {
    assertEquals(42.42, x);
  }
  %NeverOptimizeFunction(g);

  function test_osr(x, y) {
    let s = 0;
    let loop_is_turbofan = false;
    for (let i = 0; i < 20; i++) {
      %OptimizeOsr();
      s += i;
      loop_is_turbofan = %CurrentFrameIsTurbofan();
    }

    // This multiplication will trigger a deopt (because it has no feedback).
    s *= x;

    // The loop should have been in Turbofan in its last few iterations.
    assertEquals(true, loop_is_turbofan);

    // We shouldn't be in Turbofan anymore
    assertFalse(%CurrentFrameIsTurbofan());
    assertFalse(%ActiveTierIsTurbofan(test_osr));

    // Keeping the parameters alive, just to make sure that everything works.
    g(y);

    return s;
  }

  %PrepareFunctionForOptimization(test_osr);
  assertEquals(570, test_osr(3, 42.42));
}

// Testing Array.prototype.push/pop.
{
  function array_push_pop(arr, x) {
    let v = arr.pop();
    arr.push(x); // This doesn't require growing the array.
    return v + arr[5];
  }

  let arr = [0, 1, 2, 3, 4, 5];

  %PrepareFunctionForOptimization(array_push_pop);
  assertEquals(16, array_push_pop(arr, 11));

  arr[5] = 5;
  %OptimizeFunctionOnNextCall(array_push_pop);
  assertEquals(16, array_push_pop(arr, 11));
  assertOptimized(array_push_pop);

  function array_push_grow_once(arr, x, y) {
    // The 1st push will have to grow the array.
    arr.push(x);
    arr.push(y);
    return arr.at(-1) + arr.at(-2) + arr.at(-3);
  }

  arr = [0, 1, 2, 3, 4, 5];

  %PrepareFunctionForOptimization(array_push_grow_once);
  assertEquals(29, array_push_grow_once(arr, 11, 13));

  arr = [0, 1, 2, 3, 4, 5];
  %OptimizeFunctionOnNextCall(array_push_grow_once);
  assertEquals(29, array_push_grow_once(arr, 11, 13));
  assertOptimized(array_push_grow_once);
}

// Testing some number/float to int32 truncations.
{
  function truncate_number_to_int32(d, b) {
    let v1 = d ^ 42; // Checked NumberOrOddball truncation
    let p1 = b ? 4.26 : undefined;
    let v2 = p1 | 11; // (unchecked) NumberOrOddball truncation
    let p2 = b ? 3.35 : 4.59;
    let v3 = p2 & 255; // (unchecked) Float64 truncation
    return v1 + v2 + v3;
  }

  %PrepareFunctionForOptimization(truncate_number_to_int32);
  assertEquals(61, truncate_number_to_int32(1.253, 1));
  assertEquals(58, truncate_number_to_int32(1.253, 0));
  %OptimizeFunctionOnNextCall(truncate_number_to_int32);
  assertEquals(61, truncate_number_to_int32(1.253, 1));
  assertEquals(58, truncate_number_to_int32(1.253, 0));
  assertOptimized(truncate_number_to_int32);
}

// Testing construct (= new).
{
  function A() { this.x = 42; return 42; }
  %NeverOptimizeFunction(A);

  function create(c) {
    let x = { "a" : 42, c }; // Creating an object before the Construct call so
                             // that the frame state has more interesting data.
    let y = new A(c);
    return [x, y];
  }

  %PrepareFunctionForOptimization(create);
  create();
  let o1 = create();

  %OptimizeFunctionOnNextCall(create);
  let o2 = create();
  assertEquals(o1, o2);
  assertOptimized(create);

  // Triggering deopt (before the construction) by changing the target.
  let new_A_called = false;
  A = function() { new_A_called = true; }
  let o3 = create();
  assertUnoptimized(create);
  assertTrue(new_A_called);

  // Falling back to generic Construct call.
  %OptimizeFunctionOnNextCall(create);
  let o4 = create();
  assertEquals(o3, o4);
  assertOptimized(create);
}

// Testing construct (= new).
{
  function A(c) {
    if (c) {
      %DeoptimizeFunction(create_deopt);
    }
    this.x = "abc";
  }
  %NeverOptimizeFunction(A);

  function create_deopt(c) {
    let x = { "a" : 42, c }; // Creating an object before the Construct call so
                             // that the frame state has more interesting data.
    let y = new A(c);
    return [x, y];
  }

  %PrepareFunctionForOptimization(create_deopt);
  create_deopt(false);
  let o1 = create_deopt(false);

  %OptimizeFunctionOnNextCall(create_deopt);
  let o2 = create_deopt(false);
  assertEquals(o1, o2);
  assertOptimized(create_deopt);

  // Triggering deopt during the construction
  let o3 = create_deopt(true);
  assertUnoptimized(create_deopt);
  o1[0].c = true; // Fixing {o1} for the comparison, since {o3} was created with
                  // `true` as input to `create_deopt`.
  assertEquals(o1, o3);
}

// Testing SetNamedGeneric.
{
  function set_named_generic() {
    let iterator = new Set().values();
    iterator.x = 0;
    return iterator;
  }

  %PrepareFunctionForOptimization(set_named_generic);
  let before = set_named_generic();
  %OptimizeFunctionOnNextCall(set_named_generic);
  let after = set_named_generic();
  assertEquals(before, after);
  assertOptimized(set_named_generic);
}

// Testing LoadNamedGeneric.
{
  let v1 = {};

  function load_named_generic() {
    let v2 = v1.Intl;
    try {
      return v2.supportedValuesOf();
    } catch(e) {
      // Stringifying the exception for easier comparison.
      return "123" + e + v2;
    }
  }

  %PrepareFunctionForOptimization(load_named_generic);
  let before = load_named_generic();
  %OptimizeFunctionOnNextCall(load_named_generic);
  let after = load_named_generic();
  assertEquals(before, after);
  assertOptimized(load_named_generic);
}

// Testing function over- and and under-application (calling a function with
// more or fewer arguments that it expects), where the callee is inlined.
{
  function h() { return 42; }
  function g(x, y) {
    if (x == 0) {
      %DeoptimizeNow();
    }
    return x + (y | 17);
  }

  function f(x, y) {
    return h(x) + g(x) * y;
  }

  %PrepareFunctionForOptimization(h);
  %PrepareFunctionForOptimization(g);
  %PrepareFunctionForOptimization(f);
  assertEquals(108, f(5, 3));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(108, f(5, 3));
  assertOptimized(f);
  assertEquals(93, f(0, 3));
  assertUnoptimized(f);
}

// Testing function over- and under-application, where the callee is not
// inlined.
{
  %NeverOptimizeFunction(sum3);
  function sum3(a, b, c) {
    return a + b + c;
  }

  function under_apply(a, b) {
    return sum3(a, b);
  }

  %PrepareFunctionForOptimization(under_apply);
  assertEquals(NaN, under_apply(2.35, 5));

  %OptimizeFunctionOnNextCall(under_apply);
  assertEquals(NaN, under_apply(2.35, 5));
  assertOptimized(under_apply);

  function over_apply(a, b) {
    return sum3(a, b, a, b);
  }

  %PrepareFunctionForOptimization(over_apply);
  assertEquals(9.7, over_apply(2.35, 5));

  %OptimizeFunctionOnNextCall(over_apply);
  assertEquals(9.7, over_apply(2.35, 5));
  assertOptimized(over_apply);
}

// Testing const tracking let.
let glob_a = 0;
let glob_b = 3.35;
{
  function compute_val(v) { return v + 1.15; }
  function read() { return glob_a + glob_b; }
  function write(v, w) {
    glob_a = v;
    let f64 = compute_val(w);
    glob_b = f64;
  }

  %PrepareFunctionForOptimization(compute_val);

  %PrepareFunctionForOptimization(read);
  assertEquals(3.35, read());
  %OptimizeFunctionOnNextCall(read);
  assertEquals(3.35, read());
  assertOptimized(read);

  %PrepareFunctionForOptimization(write);
  // Write the same value. This won't invalidate the constness.
  write(0, 2.25);
  glob_b = 3.35;
  assertEquals(3.35, read());

  %OptimizeFunctionOnNextCall(write);
  write(0, 2.2);
  assertEquals(3.35, read());
  assertOptimized(read);

  // Invalidating {glob_a} constness.
  write(1, 2.2);
  assertUnoptimized(write);
  assertEquals(4.35, read());

  %OptimizeFunctionOnNextCall(write);
  write(1, 2.2);
  assertEquals(4.35, read());
  assertOptimized(write);
}

// Test inner functions.
{
  function fun_with_inner(x) {
    let v = 42;
    function inner() {
      v += 3;
      return x + v;
    }
    let r1 = inner();
    v += 2;
    let r2 = inner();
    return r1 + r2;
  }

  %PrepareFunctionForOptimization(fun_with_inner);
  assertEquals(105, fun_with_inner(5));
  %OptimizeFunctionOnNextCall(fun_with_inner);
  assertEquals(105, fun_with_inner(5));
}

// Testing CallWithArrayLike and CallWithSpread.
{
  function f(x, y, z) {
    return x + y + z;
  }
  %NeverOptimizeFunction(f);
  let arr = [17, 13, 5, 23];

  function f_apply(arr) {
    return f.apply(null, arr);
  }

  %PrepareFunctionForOptimization(f_apply);
  assertEquals(35, f_apply(arr));
  %OptimizeFunctionOnNextCall(f_apply);
  assertEquals(35, f_apply(arr));
  assertOptimized(f_apply);

  function f_spread(arr) {
    return f(...arr);
  }

  %PrepareFunctionForOptimization(f_spread);
  assertEquals(35, f_spread(arr));
  %OptimizeFunctionOnNextCall(f_spread);
  assertEquals(35, f_spread(arr));
  assertOptimized(f_spread);

  let small_arr = [3, 5];
  assertEquals(NaN, f_spread(small_arr));
  assertOptimized(f_spread);

  function f_forward_args() {
    return f.apply(null, arguments);
  }

  %PrepareFunctionForOptimization(f_forward_args);
  assertEquals(24, f_forward_args(12, 5, 7));
  assertEquals(24, f_forward_args(12, 5, 7, 19));
  %OptimizeFunctionOnNextCall(f_forward_args);
  assertEquals(24, f_forward_args(12, 5, 7));
  assertEquals(24, f_forward_args(12, 5, 7, 19));
  assertOptimized(f_forward_args);
}

// Testing UpdateJSArrayLength.
{
  function f(a, b, c) {
    return a + b + c;
  }

  let short_arr = [11, 27];
  function f_spread_plus_args(short_arr, x) {
    return f(...short_arr, x);
  }

  %PrepareFunctionForOptimization(f_spread_plus_args);
  assertEquals(41, f_spread_plus_args(short_arr, 3));
  %OptimizeFunctionOnNextCall(f_spread_plus_args);
  assertEquals(41, f_spread_plus_args(short_arr, 3));
  assertOptimized(f_spread_plus_args);
}

// Testing generic function call.
{
  function add3(a, b, c) { return a + b + c; }
  function add2(a, b) { return a + b; }

  function call_arg(f, a, b, c) {
    return f(a, b, c);
  }

  %PrepareFunctionForOptimization(call_arg);
  assertEquals(15, call_arg(add3, 3, 5, 7));
  assertEquals(8, call_arg(add2, 3, 5, 7));
  %OptimizeFunctionOnNextCall(call_arg);
  assertEquals(15, call_arg(add3, 3, 5, 7));
  assertEquals(8, call_arg(add2, 3, 5, 7));
  assertOptimized(call_arg);
}

// Testing Undetectable detection.
{
  function check_undetectable(x) {
    let r = x == null;
    if (x == null) return 17;
    return r;
  };

  %PrepareFunctionForOptimization(check_undetectable);
  assertEquals(false, check_undetectable(42));
  %OptimizeFunctionOnNextCall(check_undetectable);
  assertEquals(false, check_undetectable(42));
  assertOptimized(check_undetectable);
  assertEquals(17, check_undetectable(%GetUndetectable()));
  // Should deoptimize because of invalidated NoUndetectableObjects protector.
  assertUnoptimized(check_undetectable);
  %OptimizeFunctionOnNextCall(check_undetectable);
  assertEquals(17, check_undetectable(%GetUndetectable()));
  assertOptimized(check_undetectable);
}

// Testing switches.
{
  function f_switch(x) {
    switch(x) {
      // Need at least v8_flags.switch_table_min_cases (= 6) cases so that the
      // bytecode optimizes this switch with a jumptable rather than generating
      // cascading ifs/elses.
      case 3: return 3;
      case 4: return 5;
      case 5: return 7;
      case 6: return 11;
      // hole between 6 and 9
      case 9: return 13;
      // hole between 9 and 13
      case 13: return 17;
      default: return 19;
    }
  }

  %PrepareFunctionForOptimization(f_switch);
  assertEquals(3, f_switch(3));
  assertEquals(5, f_switch(4));
  assertEquals(7, f_switch(5));
  assertEquals(11, f_switch(6));
  assertEquals(13, f_switch(9));
  assertEquals(17, f_switch(13));
  // Testing holes/default case
  assertEquals(19, f_switch(0));
  assertEquals(19, f_switch(2));
  assertEquals(19, f_switch(7));
  assertEquals(19, f_switch(8));
  assertEquals(19, f_switch(10));
  assertEquals(19, f_switch(12));
  assertEquals(19, f_switch(42));

  %OptimizeFunctionOnNextCall(f_switch);
  assertEquals(3, f_switch(3));
  assertOptimized(f_switch);
  assertEquals(5, f_switch(4));
  assertEquals(7, f_switch(5));
  assertEquals(11, f_switch(6));
  assertEquals(13, f_switch(9));
  assertEquals(17, f_switch(13));
  assertOptimized(f_switch);
  // Testing holes/default case
  assertEquals(19, f_switch(0));
  assertEquals(19, f_switch(2));
  assertEquals(19, f_switch(7));
  assertEquals(19, f_switch(8));
  assertEquals(19, f_switch(10));
  assertEquals(19, f_switch(12));
  assertEquals(19, f_switch(42));
  assertOptimized(f_switch);
}

// Testing load named on numbers.
{
  function array_length(arr) {
    return arr.length;
  }

  %PrepareFunctionForOptimization(array_length);
  assertEquals(3, array_length([1, 2, 3]));
  assertEquals(undefined, array_length(3.45));
  %OptimizeFunctionOnNextCall(array_length);
  assertEquals(3, array_length([1, 2, 3]));
  assertEquals(undefined, array_length(3.45));
  assertEquals(undefined, array_length(1));
  assertOptimized(array_length);
}

// Testing load with constant string key.
{
  let o = { "a" : 42, "b": 17 };

  function load_const_key(k) {
    return o[k];
  }

  %PrepareFunctionForOptimization(load_const_key);
  assertEquals(42, load_const_key("a"));
  %OptimizeFunctionOnNextCall(load_const_key);
  assertEquals(42, load_const_key("a"));
  assertOptimized(load_const_key);
  assertEquals(17, load_const_key("b"));
  assertUnoptimized(load_const_key);
}

// Testing StoreGlobal.
{
  function store_glob() {
    glob_a = 42;
  }

  %PrepareFunctionForOptimization(store_glob);
  store_glob();
  assertEquals(glob_a, 42);
  glob_a = 25;
  %OptimizeFunctionOnNextCall(store_glob);
  store_glob();
  assertEquals(glob_a, 42);
  assertOptimized(store_glob);
}

// Testing RestLength.
{
  function rest_len(a, ...rest) {
    return rest.length;
  }

  %PrepareFunctionForOptimization(rest_len);
  assertEquals(4, rest_len("a", 1, {}, 15.25, []));
  %OptimizeFunctionOnNextCall(rest_len);
  assertEquals(4, rest_len("a", 1, {}, 15.25, []));
  assertOptimized(rest_len);
}

// Testing transitions.
{
  function transition_arr(arr) {
    var object = new Object();
    arr[0] = object;
  }

  let smi_arr = [1, 2, 3, 4];
  let holey_smi_arr = [1, 2, 3, /* hole */, 4];

  %PrepareFunctionForOptimization(transition_arr);
  transition_arr(smi_arr);
  transition_arr(holey_smi_arr);
  // Resetting the arrays to collect feedback one more time.
  smi_arr = [1, 2, 3, 4];
  holey_smi_arr = [1, 2, 3, /* hole */, 4];
  transition_arr(smi_arr);
  transition_arr(holey_smi_arr);

  let expected_smi_arr = smi_arr;
  let expected_holey_smi_arr = holey_smi_arr;

  %OptimizeFunctionOnNextCall(transition_arr);
  // Resetting the arrays
  smi_arr = [1, 2, 3, 4];
  holey_smi_arr = [1, 2, 3, /* hole */, 4];
  // Triggering transitions
  transition_arr(smi_arr);
  assertEquals(expected_smi_arr, smi_arr);
  transition_arr(holey_smi_arr);
  assertEquals(expected_holey_smi_arr, holey_smi_arr)
  assertOptimized(transition_arr);
  // Not trigering transitions (because arrays already transitioned)
  transition_arr(smi_arr);
  transition_arr(holey_smi_arr);
  assertOptimized(transition_arr);
  // Triggering deopt
  let double_arr = [1.5, 3.32, 6.28];
  transition_arr(double_arr);
  assertEquals([{}, 3.32, 6.28], double_arr);
}

// Testing map migration.
{
  function A() { this.x = 1 }
  function B() { this.x = 1 }

  function migrate_and_load(o) {
    return o.x;
  }

  let old_b1 = new B();
  let old_b2 = new B();

  old_b1.x = 42;
  old_b2.x = 186;

  %PrepareFunctionForOptimization(migrate_and_load);
  assertEquals(1, migrate_and_load(new A()));
  (new A()).x = 4.25; // Deprecate map already in feedback
  (new B()).x = 4.25; // Deprecate old_b1/old_b2 map
  assertEquals(42, migrate_and_load(old_b1));

  %OptimizeFunctionOnNextCall(migrate_and_load);
  assertEquals(42, migrate_and_load(old_b1));
  assertOptimized(migrate_and_load);
  // Calling function with deprecated map
  assertEquals(186, migrate_and_load(old_b2));
  assertOptimized(migrate_and_load);

  // Triggering deopt
  assertEquals("abc", (migrate_and_load({a: 42, x : "abc"})));
  assertUnoptimized(migrate_and_load);
}

// Testing builtin called through CallKnowJSFunction.
{
  function make_array_size(size) {
    let a = Array(size);
    assertTrue(%HasSmiOrObjectElements(a));
    assertTrue(%HasHoleyElements(a));
    assertEquals(size, a.length);
    return a;
  }

  %PrepareFunctionForOptimization(make_array_size);
  assertEquals([,,,,], make_array_size(4));
  %OptimizeFunctionOnNextCall(make_array_size);
  assertEquals([,,,,], make_array_size(4));
  assertOptimized(make_array_size);

  function make_array() {
    let a = Array();
    assertTrue(%HasSmiOrObjectElements(a));
    return a;
  }

  %PrepareFunctionForOptimization(make_array);
  assertEquals([], make_array());
  %OptimizeFunctionOnNextCall(make_array);
  assertEquals([], make_array());
  assertOptimized(make_array);
}

// Testing array destructuring.
{
  function destruct_arr () {
    [a, b] = [4, 9];
    return a + b;
  }

  %PrepareFunctionForOptimization(destruct_arr);
  assertEquals(13, destruct_arr());
  %OptimizeFunctionOnNextCall(destruct_arr);
  assertEquals(13, destruct_arr());
  assertOptimized(destruct_arr);
}

// Testing instanceof.
{
  function Foo() {}

  function test_instance_shared_struct() {
    return Foo instanceof (new SharedStructType(["foo"]));
  }

  %PrepareFunctionForOptimization(test_instance_shared_struct);
  assertFalse(test_instance_shared_struct());
  assertFalse(test_instance_shared_struct());
  assertFalse(test_instance_shared_struct());
  %OptimizeFunctionOnNextCall(test_instance_shared_struct);
  assertFalse(test_instance_shared_struct());
  assertOptimized(test_instance_shared_struct);
}

// Testing builtin continuation deopt with Math.round.
{
  function round(x, y) {
    let prev_v = y + 14.556;
    let rounded_x = Math.round(x);
    return prev_v + rounded_x;
  }

  %PrepareFunctionForOptimization(round);
  assertEquals(23.116, round(3.25569, 5.56));
  %OptimizeFunctionOnNextCall(round);
  assertEquals(23.116, round(3.25569, 5.56));
  assertOptimized(round);

  let o = { valueOf: function() { %DeoptimizeFunction(round); return 4.55 } };

  assertEquals(25.116, round(o, 5.56));
  assertUnoptimized(round);
  assertEquals(25.116, round(o, 5.56));
}

// Testing instanceof and builtin continuations.
// (copy-pasted from test/mjsunit/maglev/nested-continuations.js)
{
  function Foo() {}
  Object.defineProperty(Foo, Symbol.hasInstance, { value: Math.round });

  let foo = new Foo();

  function test_instanceof_foo(f) {
    // `f instanceof Foo` runs `%ToBoolean(Foo[Symbol.hasInstance](f))`, where
    // `Foo[Symbol.hasInstance]` is `Math.round`.
    //
    // So with sufficient builtin inlining, this will call
    // `%ToBoolean(round(%ToNumber(f)))`, which will call `f.valueOf`. If this
    // deopts (which in this test it will), we need to make sure to both round it,
    // and then convert that rounded value to a boolean.
    return f instanceof Foo;
  }

  foo.valueOf = () => {
    %DeoptimizeFunction(test_instanceof_foo);
    // Return a value which, when rounded, has ToBoolean false, and when not
    // rounded, has ToBoolean true.
    return 0.2;
  }

  %PrepareFunctionForOptimization(test_instanceof_foo);
  assertFalse(test_instanceof_foo(foo));
  assertFalse(test_instanceof_foo(foo));

  %OptimizeFunctionOnNextCall(test_instanceof_foo);
  assertFalse(test_instanceof_foo(foo));
}

// Testing Javascript builtin continuation.
{
  let arr = [1, 2, 3, 4, 5, 6];

  let trigger = false;
  function maybe_deopt(v) {
    if (v == 3 && trigger) {
      // The first time this is executed, it will change the map of {arr}, which
      // will trigger a deopt of {array_foreach}.
      arr.x = 11;
    }
    return v;
  }

  function array_foreach(arr) {
    let s = 0;
    arr.forEach((v) => s += maybe_deopt(v));
    return s;
  }


  %PrepareFunctionForOptimization(array_foreach);
  assertEquals(21, array_foreach(arr));
  %OptimizeFunctionOnNextCall(array_foreach);
  assertEquals(21, array_foreach(arr));
  assertOptimized(array_foreach);

  trigger = true;
  assertEquals(21, array_foreach(arr));
  assertUnoptimized(array_foreach);
}

// Testing CheckNumber.
{
  function check_number(x) {
    // With only number maps in the feedback, "+x" is a no-op and Maglev just
    // emits a CheckNumber.
    return +x;
  }

  %PrepareFunctionForOptimization(check_number);
  assertEquals(3.65, check_number(3.65));
  %OptimizeFunctionOnNextCall(check_number);
  assertEquals(3.65, check_number(3.65));
  assertOptimized(check_number);
  // CheckNumber should trigger a deopt for non-number maps (like strings).
  assertEquals(NaN, check_number("abc"));
  assertUnoptimized(check_number);
}

// Testing ToString.
{
  function to_string(n) {
    return String(n);
  }

  %PrepareFunctionForOptimization(to_string);
  assertEquals(String(453), to_string(453));
  %OptimizeFunctionOnNextCall(to_string);
  assertEquals(String(453), to_string(453));
  assertEquals("Symbol(abc)", to_string(Symbol("abc")));
  assertOptimized(to_string);

  let called = false;
  let o1 = { toString: function() { called = true; return 42; } };
  assertEquals("42", to_string(o1));
  assertTrue(called);
  assertOptimized(to_string);
}

// Testing NumberToString.
{
  function number_to_string(n) {
    let v = n + 5;
    return String(v);
  }

  %PrepareFunctionForOptimization(number_to_string);
  assertEquals(String(458), number_to_string(453));
  %OptimizeFunctionOnNextCall(number_to_string);
  assertEquals(String(458), number_to_string(453));
  assertOptimized(number_to_string);
}

// Testing DeleteProperty.
{
  function delete_prop_strict(o) {
    "use strict";
    delete o.x;
  }

  let o = { x : 25, y : 42 };

  %PrepareFunctionForOptimization(delete_prop_strict);
  delete_prop_strict(o);
  assertEquals({y:42}, o);

  o.x = 25;
  %OptimizeFunctionOnNextCall(delete_prop_strict);
  delete_prop_strict(o);
  assertEquals({y:42}, o);
  assertOptimized(delete_prop_strict);


  function delete_prop_sloppy(o) {
    delete o.y;
  }

  o = { x : 25, y : 42 };
  %PrepareFunctionForOptimization(delete_prop_sloppy);
  delete_prop_sloppy(o);
  assertEquals({x:25}, o);

  o.y = 42;
  %OptimizeFunctionOnNextCall(delete_prop_sloppy);
  delete_prop_sloppy(o);
  assertEquals({x:25}, o);
  assertOptimized(delete_prop_sloppy);
}

// Testing Uint32->Int32/Float64 conversion.
{
  const arr = new Uint32Array(2);
  function uint_to_i32_f64() {
    let v1 = arr[0] + 7; // requires Uint32->Int32 conversion
    let v2 = arr[1] + 3.47; // requires Uint32->Float64 conversion
    return v1 + v2;
  }

  %PrepareFunctionForOptimization(uint_to_i32_f64);
  assertEquals(10.47, uint_to_i32_f64());
  %OptimizeFunctionOnNextCall(uint_to_i32_f64),
  assertEquals(10.47, uint_to_i32_f64());
  assertOptimized(uint_to_i32_f64);

  arr[0] = 0xffffff35; // Does not fit in a signed Int32
  assertEquals(0xffffff35+7+3.47, uint_to_i32_f64());
  assertUnoptimized(uint_to_i32_f64);
}

// Testing loops in inner functions.
{
  function inner_loop(x) {
    let s = 0;
    for (let i = 0; i < x; i++) {
      s += i;
    }
    return s;
  }

  function call_loop(x) {
    return inner_loop(x);
  }

  %PrepareFunctionForOptimization(inner_loop);
  %PrepareFunctionForOptimization(call_loop);
  assertEquals(15, call_loop(6));
  %OptimizeFunctionOnNextCall(call_loop);
  assertEquals(15, call_loop(6));
  assertOptimized(call_loop);
}

// Testing ToName.
{
  function to_name() {
    var o = {
      [Symbol.toPrimitive]() {}
    };
    return o;
  }

  %PrepareFunctionForOptimization(to_name);
  let o = to_name();
  assertEquals(o, to_name());
  %OptimizeFunctionOnNextCall(to_name);
  assertEquals(o, to_name());
  assertOptimized(to_name);
}

// Testing CheckInt32IsSmi.
{
  function check_int32_is_smi (a, b) {
    let v1 = a + 1_000_000_000;
    let phi = b ? v1 : 42; // Int32 inputs for the Smi, which means that it can
                           // be untagged to Int32 if it has Int32 uses.
    let v2 = +phi; // This produces a ToNumber in the bytecode, for which Maglev
                   // inserts a CheckSmi (because of the Smi feedback). If its
                   // input is an untagged Int32 phi, this will become a
                   // CheckInt32IsSmi.
    return phi + v2; // Int32 use for the Phi so that it's untagged to Int32.
  }

  %PrepareFunctionForOptimization(check_int32_is_smi);
  assertEquals(84, check_int32_is_smi(1, false));
  %OptimizeFunctionOnNextCall(check_int32_is_smi);
  assertEquals(84, check_int32_is_smi(1, false));
  assertOptimized(check_int32_is_smi);

  // Triggering a deopt by having making {phi} not fit in Smi range
  assertEquals(4_000_000_000, check_int32_is_smi(1_000_000_000, true));
  assertUnoptimized(check_int32_is_smi);
}

// Testing creation of derived objects.
{
  class C extends Array { };

  function new_derived_obj() {
    new C();
  }

  %PrepareFunctionForOptimization(new_derived_obj);
  new_derived_obj();
  %OptimizeFunctionOnNextCall(new_derived_obj);
  new_derived_obj();
}

// Testing constructors (when it doesn't throw anything).
{
  class A extends Object {
    constructor(b) {
      super();
      this.x = b;
      this.y = 42;
    }
  }

  class B extends A {
    constructor(b) {
      super(b);
      this.z = 12;
    }
  }

  %PrepareFunctionForOptimization(B);
  let o = new B();
  %OptimizeFunctionOnNextCall(B);
  assertEquals(o, new B());
  assertOptimized(B);
}

// Testing various throws in constructors.
{
  // Testing ThrowIfNotSuperConstructor (which triggers because the base class
  // of A1 is "null", which doesn't have a constructor).
  class A1 extends null {
    constructor() {
      super();
    }
  }

  %PrepareFunctionForOptimization(A1);
  assertThrows(() => new A1(), TypeError,
               "Super constructor null of A1 is not a constructor");
  %OptimizeFunctionOnNextCall(A1);
  assertThrows(() => new A1(), TypeError,
               "Super constructor null of A1 is not a constructor");
  assertOptimized(A1);


  // Testing ThrowSuperAlreadyCalledIfNotHole (which triggers because we call
  // super() twice).
  class A2 extends Object {
    constructor() {
      super();
      super();
    }
  }

  %PrepareFunctionForOptimization(A2);
  assertThrows(() => new A2(), ReferenceError,
               "Super constructor may only be called once");
  %OptimizeFunctionOnNextCall(A2);
  assertThrows(() => new A2(), ReferenceError,
               "Super constructor may only be called once");
  assertOptimized(A2);


  // Testing ThrowSuperNotCalledIfHole (which triggers because we call
  // don't call super()).
  class A3 extends Object {
    constructor() {
    }
  }

  %PrepareFunctionForOptimization(A3);
  assertThrows(() => new A3(), ReferenceError,
               "Must call super constructor in derived class before " +
               "accessing 'this' or returning from derived constructor");
  %OptimizeFunctionOnNextCall(A3);
  assertThrows(() => new A3(), ReferenceError,
               "Must call super constructor in derived class before " +
               "accessing 'this' or returning from derived constructor");
  assertOptimized(A3);
}

// Testing closures as object properties.
{
  function create_closure(v) {
    let o = {};
    let x = 4;
    o.x = (c) => v + x++ + 2 + c;
    return o;
  }

  %PrepareFunctionForOptimization(create_closure);
  create_closure(7);
  %OptimizeFunctionOnNextCall(create_closure);
  let o = create_closure(7);
  assertEquals(7+4+2+2, o.x(2));
  assertEquals(7+5+2+5, o.x(5));
  assertOptimized(create_closure);
}

// Testing CreateShallowObjectLiteral.
{
  function g(o) { return o.u; }

  function create_shallow_obj(x) {
    var o = {u: x};
    return g(o);
  }

  %PrepareFunctionForOptimization(create_shallow_obj);
  assertEquals(42, create_shallow_obj(42));
  assertEquals(3.56, create_shallow_obj(3.56));
  %OptimizeFunctionOnNextCall(create_shallow_obj);
  assertEquals(42, create_shallow_obj(42));
  assertEquals(3.56, create_shallow_obj(3.56));
}

// Testing CheckedSmiTagFloat64.
{
  function store_f64_to_smi_field(v) {
    return {some_unique_name_U5d8Xe: Math.floor((v + 0xffffffff) - 0xfffffffe)}
           .some_unique_name_U5d8Xe;
  }

  %PrepareFunctionForOptimization(store_f64_to_smi_field);
  assertEquals(1, store_f64_to_smi_field(0));
  assertEquals(2, store_f64_to_smi_field(1));
  %OptimizeFunctionOnNextCall(store_f64_to_smi_field);
  assertEquals(2, store_f64_to_smi_field(1));
  assertOptimized(store_f64_to_smi_field);

  // Won't fit in Smi anymore, which should trigger a deopt
  assertEquals(0xffffffff+0xffffffff-0xfffffffe,
               store_f64_to_smi_field(0xffffffff));
  assertUnoptimized(store_f64_to_smi_field);
}
