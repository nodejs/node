// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function test_char_at(s, i) {
  return s.charAt(i);
}

function test_char_code_at(s, i) {
  return s.charCodeAt(i);
}

function test_code_point_at(s, i) {
  return s.codePointAt(i);
}

const functions = [
  { fn: test_char_at, oob_val: "" },
  { fn: test_char_code_at, oob_val: NaN },
  { fn: test_code_point_at, oob_val: undefined }
];

for (const { fn, oob_val } of functions) {
  %PrepareFunctionForOptimization(fn);
  fn("hello", 0);
  fn("hello", "1");
  %OptimizeFunctionOnNextCall(fn);

  // Trigger out-of-bounds speculation deopt to disable bounds speculation.
  fn("hello", 10);

  // Re-optimize with speculation disallowed (inlines with branch).
  %OptimizeFunctionOnNextCall(fn);

  // In Turbolev, negative indices should execute the out-of-bounds branch
  // and remain optimized without deoptimizing with kNotInt32.
  assertSame(oob_val, fn("hello", -1));
  assertOptimized(fn);

  // Test HeapNumber negative index as well.
  const heap_neg1 = new Float64Array([-1])[0];
  assertSame(oob_val, fn("hello", heap_neg1));
  assertOptimized(fn);
}
