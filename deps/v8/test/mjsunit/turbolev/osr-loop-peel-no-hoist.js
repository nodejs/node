// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-future --turbofan
// Flags: --no-concurrent-osr --no-maglev-hoist-osr-value-phi-untagging

// Peeling an OSR loop with the OSR-value untagging hoist disabled, exercising
// the non-hoisted conversion of the float OSR value.
function osr_peel_no_hoist(n) {
  let s = 0;
  let f = 0.5;
  for (let i = 0; i < n; i++) {
    %OptimizeOsr();
    %AssertPeeled();
    s += i;
    f += 1.5;
  }
  return s + f;
}
%PrepareFunctionForOptimization(osr_peel_no_hoist);
assertEquals(220.5, osr_peel_no_hoist(20));
