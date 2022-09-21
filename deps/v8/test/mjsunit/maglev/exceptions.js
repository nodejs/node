// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

// This examples creates a simple exception handler block where the trampoline
// has an int32 value and needs to convert to a tagged value.
function foo_int32() {
  let x = 1;
  try {
    x = x + x;
    throw "Error";
  } catch {
    return x;
  }
}
%PrepareFunctionForOptimization(foo_int32);
assertEquals(foo_int32(), 2);
%OptimizeMaglevOnNextCall(foo_int32);
assertEquals(foo_int32(), 2);

// This examples creates a simple exception handler block where the trampoline
// has an int32 value that overflows and it needs to create a HeapNumber.
function foo_int32_overflow(x) {
  try {
    x = x + x;
    throw "Error";
  } catch {
    return x;
  }
}
%PrepareFunctionForOptimization(foo_int32_overflow);
assertEquals(foo_int32_overflow(1), 2);
%OptimizeMaglevOnNextCall(foo_int32_overflow);
assertEquals(foo_int32_overflow(0x3FFFFFFF), 0x7FFFFFFE);
// If we call it with a HeapNumber, we deopt before the exception:
assertTrue(%ActiveTierIsMaglev(foo_int32_overflow));
assertEquals(foo_int32_overflow(1.1), 2.2);
assertFalse(%ActiveTierIsMaglev(foo_int32_overflow));

// This examples creates a simple exception handler block where the trampoline
// has an float64 value and needs to convert to a tagged value.
function foo_float64() {
  let x = 1.1;
  try {
    x = x + x;
    throw "Error";
  } catch {
    return x;
  }
}
%PrepareFunctionForOptimization(foo_float64);
assertEquals(foo_float64(), 2.2);
%OptimizeMaglevOnNextCall(foo_float64);
assertEquals(foo_float64(), 2.2);

// Combination of previous examples with a big number of registers.
// This creates a _quite_ large trampoline.
function foo() {
  let x = 1;
  let y = 1.1;
  let a, b, c, d, e, f, g, h;
  a = b = c = d = e = f = g = h = 0;
  let p, q, r, s, t;
  try {
    x = x + x;
    y = y + y;
    p = q = r = s = t = x;
    throw "Error";
  } catch {
    return x + y + a + b + c + d + e + f + g + h
             + p + q + r + s + t;
  }
}
%PrepareFunctionForOptimization(foo);
assertEquals(foo(), 14.2);
%OptimizeMaglevOnNextCall(foo);
assertEquals(foo(), 14.2);
