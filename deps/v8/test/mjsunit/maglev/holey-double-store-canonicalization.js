// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Storing a double into an array only canonicalizes its NaN patterns when they
// could be the hole or undefined ones. Arithmetic results never can, since
// those patterns are signalling NaNs and IEEE 754 arithmetic does not return
// one, but values read out of a Float64Array can be anything, and operations
// that manipulate bits rather than values can turn them into the patterns.

// A Float64Array lets us build any bit pattern. 0x7FF7FFFF'FFF7FFFF is the hole
// pattern with the sign bit cleared, so negating it produces the hole itself.
const bits = new Float64Array(2);
const words = new Uint32Array(bits.buffer);
words[0] = 0xFFF7FFFF;
words[1] = 0x7FF7FFFF;  // bits[0] = -hole_nan
words[2] = 0xFFF7FFFF;
words[3] = 0xFFF7FFFF;  // bits[1] = hole_nan

function makeHoley(a) {
  a[8] = 1.5;  // -> HOLEY_DOUBLE_ELEMENTS, reinterpreting the bits stored below
  return a;
}

// (1) Negating a fabricated NaN can produce the hole pattern, so the store has
// to canonicalize it.
(function() {
  function f(out, i, ta, j) { out[i] = -ta[j]; }
  const warmup = [7.5, 8.5];
  %PrepareFunctionForOptimization(f);
  f(warmup, 0, bits, 0);
  f(warmup, 0, bits, 0);
  %OptimizeMaglevOnNextCall(f);

  const out = [7.5, 8.5];
  f(out, 0, bits, 0);
  assertTrue(isNaN(out[0]));
  makeHoley(out);
  assertTrue(0 in out);
  assertTrue(isNaN(out[0]));
})();

// (2) Math.max of a value with itself can be compiled to nothing at all, so a
// fabricated hole pattern survives it unchanged.
(function() {
  function f(out, i, ta, j) { out[i] = Math.max(ta[j], ta[j]); }
  const warmup = [7.5, 8.5];
  %PrepareFunctionForOptimization(f);
  f(warmup, 0, bits, 1);
  f(warmup, 0, bits, 1);
  %OptimizeMaglevOnNextCall(f);

  const out = [7.5, 8.5];
  f(out, 0, bits, 1);
  assertTrue(isNaN(out[0]));
  makeHoley(out);
  assertTrue(0 in out);
  assertTrue(isNaN(out[0]));
})();

// (3) Arithmetic on the same value cannot produce the patterns, so nothing is
// canonicalized and the element is still a NaN.
(function() {
  function f(out, i, ta, j) { out[i] = ta[j] * 1.5; }
  const warmup = [7.5, 8.5];
  %PrepareFunctionForOptimization(f);
  f(warmup, 0, bits, 1);
  f(warmup, 0, bits, 1);
  %OptimizeMaglevOnNextCall(f);

  const out = [7.5, 8.5];
  f(out, 0, bits, 1);
  assertTrue(isNaN(out[0]));
  makeHoley(out);
  assertTrue(0 in out);
  assertTrue(isNaN(out[0]));
})();

// (4) A copy between double arrays goes through a load, which can read a
// pattern that was fabricated into the source array.
(function() {
  function f(out, i, src, j) { out[i] = src[j]; }
  const warmup = [7.5, 8.5];
  const src = [1.5, 2.5];
  %PrepareFunctionForOptimization(f);
  f(warmup, 0, src, 0);
  f(warmup, 0, src, 0);
  %OptimizeMaglevOnNextCall(f);

  const out = [7.5, 8.5];
  f(out, 0, src, 1);
  assertEquals(2.5, out[0]);
  makeHoley(out);
  assertTrue(0 in out);
  assertEquals(2.5, out[0]);
})();

// (5) Into a holey double array, a hole read out of another one is undefined
// and stays undefined, while a NaN stays a NaN.
(function() {
  function f(out, i, src, j) { out[i] = src[j]; }
  const src = [1.5, , 2.5];
  const warmup = [7.5, 8.5, , 9.5];
  %PrepareFunctionForOptimization(f);
  f(warmup, 0, src, 0);
  f(warmup, 0, src, 0);
  %OptimizeMaglevOnNextCall(f);

  const out = [7.5, 8.5, , 9.5];
  f(out, 0, src, 1);
  assertTrue(0 in out);
  assertEquals(undefined, out[0]);

  f(out, 1, bits, 1);  // a fabricated hole pattern is a NaN, not a hole
  assertTrue(1 in out);
  assertTrue(isNaN(out[1]));
})();

// (6) An arithmetic result stored into a holey double array needs no
// canonicalization either.
(function() {
  function f(out, i, a, j) { out[i] = a[j] + 1; }
  const a = [1.5, 2.5];
  const warmup = [7.5, 8.5, , 9.5];
  %PrepareFunctionForOptimization(f);
  f(warmup, 0, a, 0);
  f(warmup, 0, a, 0);
  %OptimizeMaglevOnNextCall(f);

  const out = [7.5, 8.5, , 9.5];
  f(out, 0, a, 1);
  assertEquals(3.5, out[0]);
  assertTrue(0 in out);
})();

// (7) Shrinking an array still writes a real hole into the vacated slot.
(function() {
  function f(a) { return a.pop(); }
  const warmup = [1.5, 2.5, 3.5];
  %PrepareFunctionForOptimization(f);
  f(warmup);
  f(warmup);
  %OptimizeMaglevOnNextCall(f);

  const a = [1.5, 2.5, 3.5];
  assertEquals(3.5, f(a));
  assertEquals(2, a.length);
  a.length = 3;
  assertFalse(2 in a);
  assertEquals(undefined, a[2]);
})();
