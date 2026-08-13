// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation
// Flags: --no-concurrent-osr --no-maglev-optimistic-peeled-loops

function test() {
  %OptimizeOsr();
  for (let i = 0; i < 5; i++) {
    const x = i & i;
    i = 0x80000000;
    function foo() {}
    try { Int16Array.apply(); } catch (e) {}
    new Int16Array(0x80000000, x);
  }
}
%PrepareFunctionForOptimization(test);
test();
