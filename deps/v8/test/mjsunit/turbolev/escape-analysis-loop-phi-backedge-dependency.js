// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis

function test(cond, n) {
  let outer_alloc = { f1: 0 };

  let i;
  if (cond) {
    outer_alloc.f1 = 1;
    i = 0;
  } else {
    outer_alloc.f1 = 2;
    i = 0;
  }

  while (i < n) {
    let backedge_alloc = { f2: 0 };
    // Create the dependency: backedge_alloc depends on outer_alloc
    backedge_alloc.f2 = outer_alloc;
    // backedge_alloc flows into outer_alloc's Virtual Phi backedge
    outer_alloc.f1 = backedge_alloc;
    i++;
  }

  return outer_alloc.f1;
}

%PrepareFunctionForOptimization(test);
test(true, 1);
test(false, 1);
%OptimizeFunctionOnNextCall(test);
test(true, 1);
