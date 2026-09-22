// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev

// A passing `index <unsigned array.length` bounds check proves the index is
// Smi-sized (array lengths are in [0, Smi::kMaxValue]), so the Smi-size
// artifacts (CheckInt32IsSmi / CheckedSmiSizedInt32) on the index that are
// dominated by the check can be folded away. Crucially, this may only apply to
// uses dominated by the check (same block, after it) — never to uses that
// precede it.

(function testFoldAfterBoundsCheck() {
  // The index is bounds-checked by `a[i]` and reused in a Smi context; the fold
  // should fire and results must stay correct.
  function sum(a) {
    let s = 0;
    for (let i = 0; i < a.length; i++) {
      s = (s + a[i]) | 0;
    }
    return s;
  }
  const a = new Array(64);
  for (let i = 0; i < a.length; i++) a[i] = i;
  const expected = (a.length * (a.length - 1)) / 2;

  %PrepareFunctionForOptimization(sum);
  assertEquals(expected, sum(a));
  assertEquals(expected, sum(a));
  %OptimizeFunctionOnNextCall(sum);
  assertEquals(expected, sum(a));
  assertTrue(isOptimized(sum));
})();

(function testNoFoldBeforeBoundsCheck() {
  // Regression guard for intra-block ordering. The Smi-size check on `i` from
  // the store into `smiArr` precedes the bounds check from `arr[i]` in the same
  // block, so it must NOT be folded: `i` is not yet known Smi-sized at the
  // store. A block-granular narrowing would fold it and tag a non-Smi int32
  // index without its guard, corrupting the store instead of deopting.
  function f(smiArr, arr, i) {
    i = i | 0;         // force Int32
    smiArr[0] = i;     // (A) Smi-size check on i, BEFORE the bounds check
    return arr[i];     // (B) bounds check on i against arr.length
  }
  const smiArr = [1, 2, 3];  // PACKED_SMI_ELEMENTS
  const arr = new Array(8);
  for (let k = 0; k < arr.length; k++) arr[k] = k * 10;

  %PrepareFunctionForOptimization(f);
  assertEquals(30, f(smiArr, arr, 3));
  assertEquals(30, f(smiArr, arr, 3));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(30, f(smiArr, arr, 3));
  assertTrue(isOptimized(f));

  // 2^30 is one past Smi::kMaxValue on 31-bit-Smi (pointer-compression) builds,
  // and out of bounds for `arr`. Correct forward-only fold: (A) deopts on the
  // Smi-size check before it can taint smiArr, then Ignition stores the real
  // value and `arr[2^30]` reads out of bounds. A block-granular fold would tag
  // `i` unchecked and leave garbage in smiArr[0].
  const big = 1 << 30;  // 2**30
  assertEquals(undefined, f(smiArr, arr, big));
  assertEquals(big, smiArr[0]);  // real value committed, not a truncated Smi
})();

(function testDifferentLengthDoesNotCertify() {
  // The bounds check certifies `i` against `arr`'s length; a Smi-size check on a
  // *different* value `j` must not be folded off it.
  function g(smiArr, arr, i, j) {
    i = i | 0;
    j = j | 0;
    const x = arr[i];  // bounds check on i
    smiArr[0] = j;     // Smi-size check on j (unrelated to the bounds check)
    return x + smiArr[0];
  }
  const smiArr = [0];
  const arr = new Array(8);
  for (let k = 0; k < arr.length; k++) arr[k] = k;

  %PrepareFunctionForOptimization(g);
  assertEquals(5, g(smiArr, arr, 2, 3));
  assertEquals(5, g(smiArr, arr, 2, 3));
  %OptimizeFunctionOnNextCall(g);
  assertEquals(5, g(smiArr, arr, 2, 3));
  assertTrue(isOptimized(g));

  // j is a valid int32 but not Smi-sized: its Smi-size check must still fire.
  const big = 1 << 30;
  g(smiArr, arr, 2, big);
  assertEquals(big, smiArr[0]);
})();
