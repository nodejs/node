// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// x % (2^k) strength-reduces to x & (2^k - 1) when x is provably
// non-negative; the divisor's sign is irrelevant, and the negative
// dividend and zero-divisor cases must stay correct.

function mod_in_loop(a, n) {
  let sum = 0;
  for (let i = 0; i < n; i++) {
    sum += (i + a[0]) % 256;
    a[i % 8] = sum & 0xff;
  }
  return sum;
}

%PrepareFunctionForOptimization(mod_in_loop);
const expected = mod_in_loop([1, 2, 3, 4, 5, 6, 7, 8], 40);
%OptimizeFunctionOnNextCall(mod_in_loop);
assertEquals(expected, mod_in_loop([1, 2, 3, 4, 5, 6, 7, 8], 40));

// Non-negative dividend, negative power-of-two divisor: reduces the same way
// since % follows the sign of the dividend.
function mod_neg_divisor(n) {
  let sum = 0;
  for (let i = 0; i < n; i++) {
    sum += i % -256;
  }
  return sum;
}

%PrepareFunctionForOptimization(mod_neg_divisor);
const expected_neg_divisor = mod_neg_divisor(40);
%OptimizeFunctionOnNextCall(mod_neg_divisor);
assertEquals(expected_neg_divisor, mod_neg_divisor(40));

// Negative dividend must not be folded and must keep its sign, including -0.
function mod_neg(x) {
  return x % 8;
}
%PrepareFunctionForOptimization(mod_neg);
%OptimizeFunctionOnNextCall(mod_neg);
assertEquals(-3, mod_neg(-11));
assertEquals(3, mod_neg(11));
assertEquals(-0, mod_neg(-8));
assertEquals(-Infinity, 1 / mod_neg(-8));
