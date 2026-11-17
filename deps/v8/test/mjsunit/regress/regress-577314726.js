// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function make() {
  return {x: 1, a: [[1]], b: [2], c: [3], d: [4], e: [5],
          f: [6], g: [7], h: [8], i: [9], j: [10]};
}

// Allocates the feedback vector and disables heuristic tier-up.
%PrepareFunctionForOptimization(make);
make();
assertTrue(%HasFastProperties(make()));

// Deprecate make's boilerplate map (x: Smi -> Double) and fill the new map's
// transition array (> kMaxNumberOfTransitions = 1536) so migrating the
// boilerplate normalizes it. Transitions are weak: keep `o` alive so a GC
// under gc_stress cannot clear them mid-loop.
const keep = [];
for (let n = 0; n < 1600; n++) {
  const o = {x: 1.5, ['p' + n]: 1, b: 1, c: 1, d: 1, e: 1,
             f: 1, g: 1, h: 1, i: 1, j: 1};
  keep.push(o);
}

const o1 = make();  // Migrates and normalizes the boilerplate.
assertFalse(%HasFastProperties(o1));
assertEquals([[1]], o1.a);
assertEquals([10], o1.j);

// Elements-kind feedback must not reach a sibling's AllocationSite.
o1.b[0] = 1.5;
o1.c[0] = {};
const o2 = make();
assertTrue(%HasSmiElements(o2.a[0]));
assertFalse(%HasObjectElements(o2.b));
assertFalse(%HasDoubleElements(o2.c));
for (const k of 'defghij') assertTrue(%HasSmiElements(o2[k]));
