// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation
// Flags: --no-maglev-optimistic-peeled-loops --maglev-non-eager-inlining

function inlinee() {
  return eval("1");
}
%PrepareFunctionForOptimization(inlinee);

function test() {
  for (let loop_var = 0; loop_var < 1; ) {
    ~loop_var;
    inlinee(loop_var);
    loop_var = 2.5;
  }
}

%PrepareFunctionForOptimization(test);
test();
%OptimizeMaglevOnNextCall(test);
test();
