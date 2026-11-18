// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap

// Transitioning ElementsKind (`HOLEY_SMI_ELEMENTS` -> `HOLEY_ELEMENTS`)
// must mark the map store as initializing/transitioning so
// StoreStoreElimination cannot drop it before a GC when a subsequent property
// addition overwrites the map.
function make() {
  let a = new Array(3);
  a[0] = 1;
  return a;
}

function transition_elements_and_add_prop(a, v) {
  a[0] = 1;
  a[0] = v;
  %MajorGCForCompilerTesting();
  a.prop = 1;
  return a;
}

%PrepareFunctionForOptimization(make);
%PrepareFunctionForOptimization(transition_elements_and_add_prop);
for (let i = 0; i < 3; i++) {
  let a = make();
  %MajorGCForCompilerTesting();
  transition_elements_and_add_prop(a, i);
  let b = make();
  %MajorGCForCompilerTesting();
  transition_elements_and_add_prop(b, { k: i });
}

%OptimizeFunctionOnNextCall(transition_elements_and_add_prop);
let a = make();
%MajorGCForCompilerTesting();
let res = transition_elements_and_add_prop(a, { k: 99 });
assertEquals(99, res[0].k);
assertEquals(1, res.prop);
