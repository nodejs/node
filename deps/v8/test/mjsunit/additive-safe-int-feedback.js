// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --additive-safe-int-feedback
// Flags: --turbolev --turbolev-additive-safe-int-feedback

const maxAdditiveSafeInteger = 1125899906842623; // 2^50 - 1
const minAdditiveSafeInteger = - 1125899906842624; // - 2^50

// Turbolev only emits a speculative additive-safe-integer add when the result
// is truncated to word32 (the final `| 0` case); non-truncated adds use
// Float64. That is slightly weaker than the TurboFan lowering was, so the
// non-truncated cases below only check value correctness.

// a + <constant>.
(function() {
  function foo(a) { return a + 1; }
  %PrepareFunctionForOptimization(foo);
  foo(1231234567890);
  foo(1231234567890);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1231234567891, foo(1231234567890));
  assertEquals(maxAdditiveSafeInteger, foo(maxAdditiveSafeInteger - 1));
  assertEquals(minAdditiveSafeInteger + 1, foo(minAdditiveSafeInteger));
  assertEquals(maxAdditiveSafeInteger + 1, foo(maxAdditiveSafeInteger)); // Overflow.
  assertEquals(2.5, foo(1.5));  // Double.
  assertEquals(1, foo(-0));     // Minus zero.
})();

// a + b, neither operand statically in the safe range.
(function() {
  function foo(a, b) { return a + b; }
  %PrepareFunctionForOptimization(foo);
  foo(1231234567890, 1);
  foo(1231234567890, 1);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1231234567891, foo(1231234567890, 1));
  assertEquals(maxAdditiveSafeInteger + 1, foo(maxAdditiveSafeInteger, 1));
  assertEquals(2.5, foo(1.5, 1));
})();

// a + (b | 0), truncated operand.
(function() {
  function foo(a, b) { return a + (b | 0); }
  %PrepareFunctionForOptimization(foo);
  foo(1231234567890, 1);
  foo(1231234567890, 1);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1231234567891, foo(1231234567890, 1));
  assertEquals(maxAdditiveSafeInteger + 1, foo(maxAdditiveSafeInteger, 1));
  assertEquals(2.5, foo(1.5, 1));
})();

// Chained adds.
(function() {
  function foo(a, b) { return 1 + a + b; }
  %PrepareFunctionForOptimization(foo);
  foo(1231234567890, 1);
  foo(1231234567890, 1);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1231234567892, foo(1231234567890, 1));
  assertEquals(maxAdditiveSafeInteger + 2, foo(maxAdditiveSafeInteger, 1));
  assertEquals(3.5, foo(1.5, 1));
})();

(function() {
  function foo(a, b) { return a + b + 1; }
  %PrepareFunctionForOptimization(foo);
  foo(1231234567890, 1);
  foo(1231234567890, 1);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1231234567892, foo(1231234567890, 1));
  assertEquals(maxAdditiveSafeInteger + 2, foo(maxAdditiveSafeInteger, 1));
  assertEquals(3.5, foo(1.5, 1));
})();

// (a + <constant>) | 0: the truncated result *is* lowered to a speculative
// additive-safe-integer (Int32) add. Overflow wraps mod 2^32.
(function() {
  function foo(a) { return a + 1 | 0; }
  %PrepareFunctionForOptimization(foo);
  foo(1231234567890);
  foo(1231234567890);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1231234567891 | 0, foo(1231234567890));
  assertEquals(maxAdditiveSafeInteger | 0, foo(maxAdditiveSafeInteger - 1));
  assertEquals(minAdditiveSafeInteger + 1 | 0, foo(minAdditiveSafeInteger));
  assertEquals(maxAdditiveSafeInteger + 1 | 0, foo(maxAdditiveSafeInteger)); // Overflow.
  assertEquals(2, foo(1.5));  // Double.
})();
