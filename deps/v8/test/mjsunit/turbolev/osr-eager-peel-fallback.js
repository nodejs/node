// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-future --turbofan
// Flags: --no-concurrent-osr

// OSR compiles fall back to eager loop peeling by default, composed with
// non-eager inlining.
function add(a, b) {
  return a + b;
}

function osr_entry_loop(n) {
  let s = 0;
  let f = 0.5;
  for (let i = 0; i < n; i++) {
    %OptimizeOsr();
    s = add(s, i);
    f = add(f, 1.5);
  }
  return s + f;
}
%PrepareFunctionForOptimization(add);
%PrepareFunctionForOptimization(osr_entry_loop);
assertEquals(220.5, osr_entry_loop(20));

function osr_outer_loop(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %OptimizeOsr();
    for (let j = 0; j < n; j++) {
      s = add(s, j);
    }
  }
  return s;
}
%PrepareFunctionForOptimization(osr_outer_loop);
assertEquals(3800, osr_outer_loop(20));
