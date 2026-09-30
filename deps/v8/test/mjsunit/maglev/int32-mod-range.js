// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// x % <constant> has a range bounded by the divisor's magnitude. A loop
// counter bounded by such a value is Smi-sized by construction, which lets
// the compiler drop the Smi-size check and the increment overflow check on
// the counter phi.

function fill(bytes, x) {
  const n = x % 2056;
  let sum = 0;
  for (let i = 0; i < n; i++) {
    bytes[i] = (i + x) % 256;
    sum += bytes[i];
  }
  return sum;
}

%PrepareFunctionForOptimization(fill);
const bytes = new Uint8Array(2056);
const expected = fill(bytes, 1500);
%OptimizeFunctionOnNextCall(fill);
assertEquals(expected, fill(bytes, 1500));
assertOptimized(fill);

// A negative dividend gives a negative modulus: the loop must not run.
assertEquals(0, fill(bytes, -1500));

// A zero dividend gives 0 % 2056 == 0: the loop must not run either.
assertEquals(0, fill(bytes, 0));

// A modulus of a negative constant still bounds the counter.
function fill_neg_divisor(bytes, x) {
  const n = x % -100;
  let sum = 0;
  for (let i = 0; i < n; i++) {
    sum += bytes[i] + i;
  }
  return sum;
}

%PrepareFunctionForOptimization(fill_neg_divisor);
const expected_neg = fill_neg_divisor(bytes, 99);
%OptimizeFunctionOnNextCall(fill_neg_divisor);
assertEquals(expected_neg, fill_neg_divisor(bytes, 99));

// A non-constant divisor bounds the result by the divisor's own range.
function mod_var_divisor(x, d) {
  const n = x % (d % 50 + 51);
  let sum = 0;
  for (let i = 0; i < n; i++) {
    sum += i;
  }
  return sum;
}

%PrepareFunctionForOptimization(mod_var_divisor);
const expected_var = mod_var_divisor(1000, 30);
%OptimizeFunctionOnNextCall(mod_var_divisor);
assertEquals(expected_var, mod_var_divisor(1000, 30));

// The -0 result and zero-divisor deopt paths must stay correct.
function mod_minus_zero(x, d) {
  return Object.is(x % d, -0);
}

%PrepareFunctionForOptimization(mod_minus_zero);
assertFalse(mod_minus_zero(10, 4));
assertFalse(mod_minus_zero(8, 4));
%OptimizeFunctionOnNextCall(mod_minus_zero);
assertFalse(mod_minus_zero(10, 4));
assertTrue(mod_minus_zero(-8, 4));
assertFalse(mod_minus_zero(8, 0));
assertTrue(Number.isNaN(8 % 0));
