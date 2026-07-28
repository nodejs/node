// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-future --turbofan

// The Maglev truncation pass also runs in the Turbolev frontend: an
// Int32MultiplyWithOverflow consumed only with ToInt32 semantics is folded to a
// plain Int32Multiply when a refined input range proves the product stays in the
// safe-integer range, including when it feeds a truncatable add/subtract chain.

// `xh * l` can be minus zero (xh < 0, l == 0); masked, so the check is dropped.
function mul_masked(x, e) {
  let xh = x >> 14, l = e & 0x3fff;
  return (xh * l) & 0xff;
}
%PrepareFunctionForOptimization(mul_masked);
mul_masked(268435455, 12345);
mul_masked(1000, 2000);
%OptimizeFunctionOnNextCall(mul_masked);
assertEquals(0, mul_masked(-268435455, 0));
assertOptimized(mul_masked);

// The product feeds a truncatable add.
function mul_through_add(x, e, y, f) {
  let xh = x >> 14, l = e & 0x3fff;
  let yh = y >> 14, m = f & 0x3fff;
  return (xh * l + yh * m) & 0xff;
}
%PrepareFunctionForOptimization(mul_through_add);
mul_through_add(268435455, 12345, 268435455, 12345);
mul_through_add(1000, 2000, 3000, 4000);
%OptimizeFunctionOnNextCall(mul_through_add);
assertEquals(199, mul_through_add(-268435455, 0, 268435455, 12345));
assertOptimized(mul_through_add);

// The product feeds a truncatable subtract.
function mul_through_sub(x, e, y, f) {
  let xh = x >> 14, l = e & 0x3fff;
  let yh = y >> 14, m = f & 0x3fff;
  return (yh * m - xh * l) & 0xff;
}
%PrepareFunctionForOptimization(mul_through_sub);
mul_through_sub(268435455, 12345, 268435455, 12345);
mul_through_sub(1000, 2000, 3000, 4000);
%OptimizeFunctionOnNextCall(mul_through_sub);
assertEquals(199, mul_through_sub(-268435455, 0, 268435455, 12345));
assertOptimized(mul_through_sub);

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
%OptimizeFunctionOnNextCall(mul_in_loop);
assertEquals(0, mul_in_loop(5, -268435455, 0));
assertOptimized(mul_in_loop);

// The product is observed as a value (not truncated), so the minus-zero check
// must be kept: -0 stays -0 and +0 stays +0.
function mul_observed(x, e) {
  let xh = x >> 14, l = e & 0x3fff;
  return 1 / (xh * l);
}
%PrepareFunctionForOptimization(mul_observed);
mul_observed(268435455, 12345);
mul_observed(1000, 2000);
%OptimizeFunctionOnNextCall(mul_observed);
assertEquals(-Infinity, mul_observed(-268435455, 0));
assertEquals(Infinity, mul_observed(268435455, 0));
