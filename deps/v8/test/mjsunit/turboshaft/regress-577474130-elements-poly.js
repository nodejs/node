// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap

// Polymorphic elements-kind transitions (TransitionElementsKind) must mark
// the map store as initializing/transitioning so StoreStoreElimination cannot
// eliminate the transition before an intervening GC when a subsequent store
// transitions the map further.
function store_poly(a, v) {
  a[0] = v;
}

const keep = [];
function test_poly_param(a, v) {
  a.tag;
  a[0] = 1;
  store_poly(a, v);
  %MajorGCForCompilerTesting();
  a.prop = 1;
  return a;
}

%PrepareFunctionForOptimization(store_poly);
for (let i = 0; i < 3; i++) {
  let a_keep = new Array(3);
  a_keep.tag = 1;
  a_keep[0] = 1;
  keep.push(a_keep);

  let a = new Array(3);
  a.tag = 1;
  a[0] = 1;
  store_poly(a, i);

  let a2 = new Array(3);
  a2.tag = 1;
  a2[0] = 1;
  store_poly(a2, { k: i });

  let b = { 0: 1, tag: 1 };
  keep.push(b);
  store_poly(b, { k: i });
}

%PrepareFunctionForOptimization(test_poly_param);
for (let i = 0; i < 3; i++) {
  let x = new Array(3);
  x.tag = 1;
  x[0] = 1;
  test_poly_param(x, i);

  let y = new Array(3);
  y.tag = 1;
  y[0] = 1;
  test_poly_param(y, { k: i });
}

%OptimizeFunctionOnNextCall(test_poly_param);
let x = new Array(3);
x.tag = 1;
x[0] = 1;
%MajorGCForCompilerTesting();
let r = test_poly_param(x, { k: 99 });
assertEquals(99, r[0].k);
assertEquals(1, r.prop);
