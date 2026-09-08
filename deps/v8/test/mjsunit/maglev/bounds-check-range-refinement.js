// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev

// A passing `index <unsigned length` check bounds the index for every node the
// optimizer visits afterwards in the same block, which lets overflow-checked
// arithmetic on the index be downgraded and its Smi-size checks be dropped.
// Uses that precede the check must keep their checks.

(function testIndexIncrementAfterBoundsCheck() {
  // Mirrors sjcl/jsbn `am3`: the index is bounds-checked, then post-incremented
  // and used again, all inside one loop body.
  function am(a, i, w, j, n) {
    let c = 0;
    while (--n >= 0) {
      const l = a[i] & 0x3fff;
      const h = a[i++] >> 14;
      const m = h * l + c;
      w[j++] = m & 0xfffffff;
      c = m >> 28;
    }
    return c;
  }

  function run() {
    const a = [];
    for (let k = 0; k < 32; k++) a[k] = (k * 2654435761) | 0;
    const w = new Array(32).fill(0);
    const c = am(a, 0, w, 0, 32);
    return [c, w.join(',')];
  }

  %PrepareFunctionForOptimization(am);
  const expected = run();
  run();
  %OptimizeFunctionOnNextCall(am);
  assertEquals(expected, run());
  assertEquals(expected, run());
})();

(function testUseBeforeCheckKeepsOverflowCheck() {
  // `i + 1` is evaluated before `a[i]`'s bounds check, so it must not assume i
  // is in bounds. When the check deopts, the already-computed sum has to be the
  // exact (non-wrapping) value.
  function f(a, i) {
    const t = i + 1;
    const v = a[i];
    return [t, v];
  }

  const a = [10, 20, 30];
  %PrepareFunctionForOptimization(f);
  assertEquals([1, 10], f(a, 0));
  assertEquals([3, 30], f(a, 2));
  %OptimizeFunctionOnNextCall(f);
  assertEquals([3, 30], f(a, 2));
  // Out of bounds: the load deopts, but `t` must still be 2^30, not wrapped.
  assertEquals([1073741824, undefined], f(a, 1073741823));
})();

(function testOutOfBoundsAndNegativeIndices() {
  function get(a, i) {
    return a[i];
  }
  const a = [1, 2, 3];
  %PrepareFunctionForOptimization(get);
  assertEquals(2, get(a, 1));
  assertEquals(3, get(a, 2));
  %OptimizeFunctionOnNextCall(get);
  assertEquals(2, get(a, 1));
  assertEquals(undefined, get(a, 3));
  assertEquals(undefined, get(a, -1));
  assertEquals(undefined, get(a, 4294967295));
})();

(function testEmptyArrayNeverRefines() {
  // length 0: no index can pass the check, so nothing may be assumed.
  function get(a, i) {
    return a[i];
  }
  const empty = [];
  %PrepareFunctionForOptimization(get);
  assertEquals(undefined, get(empty, 0));
  assertEquals(undefined, get(empty, 1));
  %OptimizeFunctionOnNextCall(get);
  assertEquals(undefined, get(empty, 0));
})();

(function testIndexReusedInSmiContextAfterCheck() {
  // After the check, `i` is known Smi-sized, so the Smi-size check on the
  // stored index can go; the stored values must stay correct.
  function copyIndices(a, out) {
    for (let i = 0; i < a.length; i++) {
      const v = a[i];
      out[i] = i + v;
    }
    return out;
  }
  const a = new Array(48);
  for (let i = 0; i < a.length; i++) a[i] = i * 3;
  const expected = a.map((v, i) => i + v);

  %PrepareFunctionForOptimization(copyIndices);
  assertEquals(expected, copyIndices(a, new Array(48)));
  assertEquals(expected, copyIndices(a, new Array(48)));
  %OptimizeFunctionOnNextCall(copyIndices);
  assertEquals(expected, copyIndices(a, new Array(48)));
})();
