// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Regression test for crbug.com/503622403. Turboshaft BuildGraph must not
// fail the dominating_frame_state.valid() DCHECK when compiling a function
// that inlines Array.prototype.sort with a comparator that contains a
// try-catch.

function cmp(a, b) {
  try {
    return a - b;
  } catch(e) {}
}

function f() {
  return [3, 1].sort(cmp);
}

%PrepareFunctionForOptimization(cmp);
%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f();
