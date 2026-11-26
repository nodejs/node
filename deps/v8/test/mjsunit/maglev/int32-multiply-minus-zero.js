// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// A zero product of Int32 inputs is -0 when either input is negative, and
// Int32MultiplyWithOverflow deopts to produce it; when neither is negative
// the product is +0 and the code stays. The edges are the inputs whose sign
// is only in bit 31, which need not be Smis, so `| 0` makes them Int32.

let count = 0;
function compile() {
  // Each case compiles its own source, since a -0 result changes the
  // multiplication's feedback.
  const mul =
      new Function('a', 'b', `return (a | 0) * (b | 0);  // ${count++}`);
  %PrepareFunctionForOptimization(mul);
  mul(3.5, 4.5);  // Number feedback for the `| 0`s, so no Smi checks.
  mul(5, 6);
  %OptimizeMaglevOnNextCall(mul);
  mul(7, 8);
  assertMaglevved(mul);
  return mul;
}

for (const [a, b] of [[0, 0], [0, 1], [1, 0], [0, 0x7fffffff],
                      [0x7fffffff, 0]]) {
  const mul = compile();
  assertTrue(Object.is(mul(a, b), 0));
  assertMaglevved(mul);
}

for (const [a, b] of [[0, -1], [-1, 0], [0, -0x7fffffff], [-0x7fffffff, 0],
                      [0, -0x80000000], [-0x80000000, 0]]) {
  const mul = compile();
  assertTrue(Object.is(mul(a, b), -0));
}
