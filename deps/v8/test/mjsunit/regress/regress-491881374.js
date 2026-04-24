// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Turboshaft LoopUnrollingReducer miscompilation PoC
// Bug: GetIterCountIfStaticCanonicalForLoop in loop-unrolling-reducer.cc
// handles the right-phi case (phi on RIGHT side of Sub: `i = c - i`)
// incorrectly. CountIterations simulates `i - c` instead of `c - i`,
// causing wrong loop iteration count and incorrect full unrolling.

// Test 1: Basic miscompilation (phi on RIGHT side of Sub)
function test_right_phi() {
  let count = 0;
  for (let i = 2; i >= 0; i = (3 - i) | 0) {
    // Real: i oscillates 2 -> 1 -> 2 -> 1 -> ... (always >= 0, runs 5 times)
    // Bug: analysis computes i: 2 -> 2-3=-1 (exits after 1 iter, unrolls wrong)
    count = (count + 1) | 0;
    if (count >= 5) break;
  }
  return count;  // Should be 5, JIT returns 2
}

// Test 2: Control - phi on LEFT side works correctly
function test_left_phi() {
  let count = 0;
  for (let i = 0; i < 5; i = (i + 1) | 0) {
    count = (count + 1) | 0;
  }
  return count;  // Should be 5, correctly returns 5
}

// Test 3: Decrement pattern - JIT value is LARGER than correct
function test_decrement() {
  let remaining = 10;
  for (let i = 2; i >= 0; i = (3 - i) | 0) {
    remaining = (remaining - 1) | 0;
    if (remaining <= 0) break;
  }
  return remaining;  // Should be 0, JIT returns 8
}

// Test 4: OverflowCheckedSub path (without |0)
function test_overflow_checked() {
  let count = 0;
  let i = 2;
  while (i >= 0) {
    count++;
    i = 3 - i;  // No |0, uses OverflowCheckedSub path
    if (count >= 5) break;
  }
  return count;  // Should be 5, JIT returns 2
}

let tests = [
  ["right_phi", test_right_phi],
  ["left_phi (control)", test_left_phi],
  ["decrement", test_decrement],
  ["overflow_checked", test_overflow_checked],
];

for (let [name, fn] of tests) {
  %PrepareFunctionForOptimization(fn);
  let interp = fn();
  fn();
  %OptimizeFunctionOnNextCall(fn);
  let opt = fn();
  assertEquals(interp, opt);
}
