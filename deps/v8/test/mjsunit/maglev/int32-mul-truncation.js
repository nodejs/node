// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-truncation

// An Int32MultiplyWithOverflow whose result is only consumed with ToInt32
// semantics is folded to a plain Int32Multiply when a refined input range
// proves the product stays in the safe-integer range, dropping its overflow and
// minus-zero checks. A truncatable Int32(Add|Subtract) propagates truncatability
// to its inputs, so the fold also fires when the product feeds an add/subtract
// chain.

// `xh * l` can be minus zero (xh < 0, l == 0); masked, so the check is dropped
// and the minus zero is observed as 0 without deopting.
function mul_masked(x, e) {
  let xh = x >> 14, l = e & 0x3fff;
  return (xh * l) & 0xff;
}
%PrepareFunctionForOptimization(mul_masked);
mul_masked(268435455, 12345);
mul_masked(1000, 2000);
%OptimizeMaglevOnNextCall(mul_masked);
assertEquals(0, mul_masked(-268435455, 0));
assertTrue(isMaglevved(mul_masked));

// The product feeds a truncatable add, so the multiplication still folds.
function mul_through_add(x, e, y, f) {
  let xh = x >> 14, l = e & 0x3fff;
  let yh = y >> 14, m = f & 0x3fff;
  return (xh * l + yh * m) & 0xff;
}
%PrepareFunctionForOptimization(mul_through_add);
mul_through_add(268435455, 12345, 268435455, 12345);
mul_through_add(1000, 2000, 3000, 4000);
%OptimizeMaglevOnNextCall(mul_through_add);
assertEquals(199, mul_through_add(-268435455, 0, 268435455, 12345));
assertTrue(isMaglevved(mul_through_add));

// The product feeds a truncatable subtract.
function mul_through_sub(x, e, y, f) {
  let xh = x >> 14, l = e & 0x3fff;
  let yh = y >> 14, m = f & 0x3fff;
  return (yh * m - xh * l) & 0xff;
}
%PrepareFunctionForOptimization(mul_through_sub);
mul_through_sub(268435455, 12345, 268435455, 12345);
mul_through_sub(1000, 2000, 3000, 4000);
%OptimizeMaglevOnNextCall(mul_through_sub);
assertEquals(199, mul_through_sub(-268435455, 0, 268435455, 12345));
assertTrue(isMaglevved(mul_through_sub));

// The fold fires inside a loop.
function mul_in_loop(n, x, e) {
  let acc = 0;
  for (let i = 0; i < n; i++) {
    let xh = x >> 14, l = e & 0x3fff;
    acc = (acc + xh * l) & 0x3fffffff;
  }
  return acc;
}
%PrepareFunctionForOptimization(mul_in_loop);
mul_in_loop(5, 268435455, 12345);
mul_in_loop(5, 1000, 2000);
%OptimizeMaglevOnNextCall(mul_in_loop);
assertEquals(0, mul_in_loop(5, -268435455, 0));
assertTrue(isMaglevved(mul_in_loop));

// The product is observed as a value (not truncated), so the minus-zero check
// must be kept: -0 stays -0 and +0 stays +0.
function mul_observed(x, e) {
  let xh = x >> 14, l = e & 0x3fff;
  return 1 / (xh * l);
}
%PrepareFunctionForOptimization(mul_observed);
mul_observed(268435455, 12345);
mul_observed(1000, 2000);
%OptimizeMaglevOnNextCall(mul_observed);
assertEquals(-Infinity, mul_observed(-268435455, 0));
assertEquals(Infinity, mul_observed(268435455, 0));

// The crypto am3 pattern: `xh * l` and `h * xl` feed a truncated add.
function mul_mask_shift(x, e0, e1, w0) {
  var xl = x & 0x3fff, xh = x >> 14;
  var l = e0 & 0x3fff;
  var h = e1 >> 14;
  var m = xh * l + h * xl;
  return ((m & 0x3fff) << 14) + (m >> 14) + (w0 | 0);
}
%PrepareFunctionForOptimization(mul_mask_shift);
const r1 = mul_mask_shift(268435455, 12345, 67890, 7);
const r2 = mul_mask_shift(-268435455, 0, 0, 7);
%OptimizeMaglevOnNextCall(mul_mask_shift);
assertEquals(r1, mul_mask_shift(268435455, 12345, 67890, 7));
assertEquals(r2, mul_mask_shift(-268435455, 0, 0, 7));

// Two 28-bit masked inputs give a product range above 2^53, where the
// Number product rounds and no longer matches a wrapping int32 multiply. The
// fold must not fire, so the result stays the ToInt32 of the rounded double
// product. Warm up with small (smi-product) values so the multiply is built as
// an Int32MultiplyWithOverflow before the large product is seen. The mask keeps
// the low bits, where a wrongly-wrapped product would diverge from the rounded
// one.
function mul_beyond_safe_int(x, y) {
  let a = x & 0x0fffffff, b = y & 0x0fffffff;
  return (a * b) & 0x7fffffff;
}
%PrepareFunctionForOptimization(mul_beyond_safe_int);
mul_beyond_safe_int(1234, 5678);
mul_beyond_safe_int(4321, 8765);
%OptimizeMaglevOnNextCall(mul_beyond_safe_int);
assertEquals(((0x0fffffff * 0x0fffffff) & 0x7fffffff),
             mul_beyond_safe_int(0x0fffffff, 0x0fffffff));

// Two 18-bit masked inputs give a product range above int32 but within the
// safe-integer range, so the fold still fires: the overflow check is dropped and
// the large product no longer deopts.
function mul_above_int32(x, y) {
  let a = x & 0x3ffff, b = y & 0x3ffff;
  return (a * b) & 0x3fffffff;
}
%PrepareFunctionForOptimization(mul_above_int32);
mul_above_int32(3, 5);
mul_above_int32(100, 200);
%OptimizeMaglevOnNextCall(mul_above_int32);
assertEquals(((0x3ffff * 0x3ffff) & 0x3fffffff),
             mul_above_int32(0x3ffff, 0x3ffff));
assertTrue(isMaglevved(mul_above_int32));
