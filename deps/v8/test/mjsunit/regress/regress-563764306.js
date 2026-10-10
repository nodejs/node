// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --log-deopt --logfile='+'

// Optimized code weakly references the maps it was specialized for. When such
// a map dies, the GC marks the dependent code for deoptimization from a
// background thread which has no LocalHeap, so logging that event must not
// create handles.

function load(obj) {
  return obj.c;
}

for (let i = 0; i < 200; i++) {
  %PrepareFunctionForOptimization(load);
  load({a: i, b: i});
  load({c: i});
  %OptimizeFunctionOnNextCall(load);
  if (i % 5 === 0) gc();
}
