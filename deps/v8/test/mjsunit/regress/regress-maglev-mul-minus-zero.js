// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Maglev's Int32 constant folding of `*` must not turn a -0 product into +0.
// Each case needs its own copy of the callee so that the `*` feedback stays
// SignedSmall (a -0 result anywhere would widen it to Number and take the
// Float64 path instead), which is what makes the multiply reach
// TryFoldInt32BinaryOperation(int32_t, int32_t) with two constants.

function makeMul(name) {
  // The case name is interpolated so that each call compiles a distinct
  // source string: identical sources hit the eval compilation cache, which
  // would share one callee, and with it one feedback vector, across cases.
  const src = `(function mul(a, b, stop) {
     // ${name}
     if (stop) return -Infinity;
     return a * b;
   })`;
  const mul = eval(src);
  %PrepareFunctionForOptimization(mul);
  mul(2, 3, false);  // SignedSmall feedback, product is not -0.
  mul(2, 3, false);
  return mul;
}

function check(name, x, y, expected) {
  const mul = makeMul(name);
  const f = new Function('stop', 'mul', `return 1 / mul(${x}, ${y}, stop);`);
  %PrepareFunctionForOptimization(f);
  f(true, mul);  // Call-site feedback without executing the `*`.
  f(true, mul);
  %OptimizeMaglevOnNextCall(f);
  assertEquals(expected, f(false, mul), name);
}

// -0 cases: 1 / -0 is -Infinity.
check('-1 * 0', -1, 0, -Infinity);
check('0 * -1', 0, -1, -Infinity);
check('-3 * 0', -3, 0, -Infinity);
check('INT32_MIN * 0', -2147483648, 0, -Infinity);
check('0 * INT32_MIN', 0, -2147483648, -Infinity);

// +0 products must still fold.
check('3 * 0', 3, 0, Infinity);
check('0 * 0', 0, 0, Infinity);

function mulKeep(a, b, stop) {
  if (stop) return -Infinity;
  return a * b;
}
%PrepareFunctionForOptimization(mulKeep);
mulKeep(2, 3, false);
mulKeep(2, 3, false);

// Non-zero product: folding is unaffected and no deopt should happen.
function product(stop) {
  const x = -3;
  const y = -4;
  return mulKeep(x, y, stop);
}
%PrepareFunctionForOptimization(product);
product(true);
product(true);
%OptimizeMaglevOnNextCall(product);
assertEquals(12, product(false));
assertOptimized(product);
