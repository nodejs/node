// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Regression test for crbug.com/503557995.  The inlined Array.prototype.sort
// must remain heap-correct when the user-provided comparator mutates the
// receiver during the sort: pushing to grow the backing store, truncating
// length back, mutating prototypes, throwing, etc.  Each scenario below
// exercises one of these paths.

'use strict';

function gcChurn(level) {
  const g = [];
  const n = 20 + (level * 30);
  for (let i = 0; i < n; i++) {
    g.push({a: i, b: new Array(32 + (i & 31)).fill(i)});
  }
}

function makeArray(kind) {
  if (kind === 0) return [3, 1, 2];
  if (kind === 1) return [{v: 3}, {v: 1}, {v: 2}];
  return [3.3, 1.1, 2.2];
}

const scenarios = [
  // Grow + truncate during cmp.
  function(arr, state) {
    if (!state.once) {
      state.once = true;
      for (let i = 0; i < 64; i++) arr.push(1000 + i);
      arr.length = 3;
    }
    gcChurn(1);
    return (arr[0] > arr[1]) ? 1 : -1;
  },
  // Re-entrant sort during cmp.
  function(arr, state) {
    if (!state.once) {
      state.once = true;
      try { arr.sort((x, y) => (x > y ? -1 : 1)); } catch (_) {}
    }
    gcChurn(0);
    return (arr[0] > arr[1]) ? 1 : -1;
  },
  // Boxed valueOf result that mutates on coercion.
  function(arr, state) {
    if (!state.once) {
      state.once = true;
      return { valueOf() {
        for (let i = 0; i < 40; i++) arr.push(i);
        arr.length = 3;
        gcChurn(2);
        return -1;
      }};
    }
    return -1;
  },
  // Throwing cmp.
  function(arr, state) {
    if (!state.once) { state.once = true; throw new Error('boom'); }
    return -1;
  },
  // Prototype swap during cmp.
  function(arr, state) {
    if (!state.once) {
      state.once = true;
      const p = Object.getPrototypeOf(arr);
      try {
        Object.setPrototypeOf(arr, []);
        Object.setPrototypeOf(arr, p);
      } catch (_) {}
      for (let i = 0; i < 24; i++) arr.push(i);
      arr.length = 3;
    }
    return -1;
  },
  // Bulk push + truncate.
  function(arr, state) {
    if (!state.once) {
      state.once = true;
      const t = [];
      for (let i = 0; i < 300; i++) t.push(i | 0);
      arr.push.apply(arr, t);
      arr.length = 3;
    }
    gcChurn(2);
    return -1;
  },
];

function numCmp(a, b) { return a - b; }
function objCmp(a, b) { return a.v - b.v; }

let arr;
let state;
let scenario;
let kind;

function cmp(a, b) {
  if (kind === 1) return objCmp(a, b);
  const r = scenario(arr, state, a, b);
  if (typeof r === 'object') return r;
  if (kind === 2) {
    const x = Number(a), y = Number(b);
    if (Number.isNaN(x) || Number.isNaN(y)) return 0;
    return x < y ? -1 : x > y ? 1 : 0;
  }
  return Number(r) || numCmp(a, b);
}

function f() {
  arr = makeArray(kind);
  state = {once: false};
  try { arr.sort(cmp); } catch (_) {}
  return arr.length;
}

%PrepareFunctionForOptimization(f);
for (let s = 0; s < scenarios.length; s++) {
  for (let k = 0; k < 3; k++) {
    scenario = scenarios[s];
    kind = k;
    for (let i = 0; i < 60; i++) f();
    %OptimizeFunctionOnNextCall(f);
    for (let i = 0; i < 200; i++) assertEquals(3, f());
    %DeoptimizeFunction(f);
  }
}
