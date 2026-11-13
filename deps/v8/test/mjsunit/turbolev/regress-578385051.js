// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev-range-verification

// 1. DAG merge where one incoming path lacks an entry in NodeRanges (due to an
// unfolded dead branch before CheckedSmiUntag) while the other narrows `i` to
// [0, 3]. Joining must drop the non-shared key (widening back to static range)
// rather than keeping [0, 3] and eliding the array bounds check.
(function testDagRangeJoin() {
  function f(b, i, a) {
    let r = 0;
    b = b & 7;
    M: {
      H: {
        if (b < -7) break H;
        if (i < 0) break H;
        if (i > 3) break H;
        break M;
      }
      r = 1;
    }
    if (a.length > 3) a[i] = 9.5;
    return r;
  }

  const a8 = [1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5];
  const a4 = [1.5, 2.5, 3.5, 4.5];

  %PrepareFunctionForOptimization(f);
  for (let n = 0; n < 10; n++) {
    f(n, n & 3, a4);
    f(n, 4 + (n & 3), a8);
  }
  %OptimizeFunctionOnNextCall(f);
  assertEquals(0, f(1, 2, a4));
  assertEquals(9.5, a4[2]);

  assertEquals(1, f(1, 100000000, a4));
  assertEquals(100000001, a4.length);
  assertEquals(9.5, a4[100000000]);
})();

// 2. Loop fixpoint interaction: a pre-loop merge drops `i`'s narrowed range,
// and an in-loop branch (`i <= p`) narrows `i` against a loop phi `p` that
// starts at [0, 0] on the first pass and widens on the second pass.
(function testLoopFixpointRangeJoin() {
  function f_loop(b, i, a) {
    let r = 0;
    b = b & 7;
    M: {
      H: {
        if (b < -7) break H;
        if (i < 0) break H;
        if (i > 3) break H;
        break M;
      }
      r = 1;
    }
    for (let p = 0; p < 8; p++) {
      if (i <= p && a.length > 3) {
        a[i] = 9.5;
      }
    }
    return r;
  }

  const b8 = [1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5];
  const b4 = [1.5, 2.5, 3.5, 4.5];

  %PrepareFunctionForOptimization(f_loop);
  for (let n = 0; n < 10; n++) {
    f_loop(n, n & 3, b4);
    f_loop(n, 4 + (n & 3), b8);
  }
  %OptimizeFunctionOnNextCall(f_loop);
  assertEquals(0, f_loop(1, 2, b4));
  assertEquals(9.5, b4[2]);

  assertEquals(1, f_loop(1, 6, b4));
  assertEquals(7, b4.length);
  assertEquals(9.5, b4[6]);
})();

// 3. One-sided equality narrowing (`Operation::kStrictEqual` only inserts into
// the `if_true` RangeMap) joined with an incoming path lacking an entry in
// NodeRanges.
(function testEqualityNarrowingRangeJoin() {
  function f_eq(b, i, a) {
    let r = 0;
    b = b & 7;
    M: {
      H: {
        if (b < -7) break H;
        if (i === 2) break M;
      }
      r = 1;
    }
    if (a.length > 3) a[i] = 9.5;
    return r;
  }

  const c8 = [1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5];
  const c4 = [1.5, 2.5, 3.5, 4.5];

  %PrepareFunctionForOptimization(f_eq);
  for (let n = 0; n < 10; n++) {
    f_eq(n, n & 3, c4);
    f_eq(n, 4 + (n & 3), c8);
  }
  %OptimizeFunctionOnNextCall(f_eq);
  assertEquals(0, f_eq(1, 2, c4));
  assertEquals(9.5, c4[2]);

  assertEquals(1, f_eq(1, 100000000, c4));
  assertEquals(100000001, c4.length);
  assertEquals(9.5, c4[100000000]);
})();

// 4. Resumable generator loop: smoke-tests `ProcessPhis` +
// `InitializeLoopPhisIfMissing` in `PreProcessBasicBlock` when resuming at
// `yield` inside the loop body.
(function testResumableLoopRangeJoin() {
  function* gen(mode, i, a) {
    let idx = i | 0;
    if (mode === 0) {
      if (idx < 0 || idx > 3) return -1;
    }
    for (let k = 0; k < 3; k++) {
      if (k > 0 && a.length > 3) {
        a[idx] = 9.5;
      }
      yield k;
    }
    return 0;
  }

  const d8 = [1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5];
  const d4 = [1.5, 2.5, 3.5, 4.5];

  // Create a suspended generator in Ignition with out-of-bounds idx = 6.
  const suspended = gen(1, 6, d4);
  assertEquals({value: 0, done: false}, suspended.next());

  // Warm up and optimize `gen`.
  %PrepareFunctionForOptimization(gen);
  for (let n = 0; n < 10; n++) {
    for (const _ of gen(0, n & 3, d4)) {}
    for (const _ of gen(1, 4 + (n & 3), d8)) {}
  }
  %OptimizeFunctionOnNextCall(gen);
  for (const _ of gen(0, 2, d4)) {}

  // Resume the suspended generator with idx = 6 in optimized Turbolev code.
  assertEquals({value: 1, done: false}, suspended.next());
  assertEquals(7, d4.length);
  assertEquals(9.5, d4[6]);
})();
