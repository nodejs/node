// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Test sinking of Phi input tagging out of a branching block into its
// edge-split block.

function test(f, ...checks) {
  const run = () => {
    for (const [expected, ...args] of checks) assertSame(expected, f(...args));
  };
  %PrepareFunctionForOptimization(f);
  run();
  run();
  %OptimizeMaglevOnNextCall(f);
  run();
  %OptimizeFunctionOnNextCall(f);
  run();
}

// Float64 over `||`, `&&` (preserving -0 and NaN), and post-merge `+`.
const float64_or = x => Math.round(x / 4) || 32;
test(float64_or, [32, 0.5], [32, -0.5], [32, NaN], [3, 12], [2.5e10, 1e11],
     [Infinity, Infinity]);

const float64_and = x => Math.round(x / 4) && 32;
test(float64_and, [0, 0.5], [-0, -0.5], [NaN, NaN], [32, 12], [32, 1e11]);

const float64_or_then_add = (x, y) => y + (Math.round(x / 4) || 32);
test(float64_or_then_add, [33, 0.5, 1], [33, -0.5, 1], [32.5, NaN, 0.5],
     [4, 12, 1], [2.5e10 + 1, 1e11, 1]);

// Non-Smi Int32 and Uint32.
const int32_or = (a, b) => ((a + b) | 0) || 7;
test(int32_or, [7, 0, 0], [3, 1, 2], [0x7fffffff, 0x7ffffff0, 0xf],
     [-0x80000000, 0x7fffffff, 1], [7, 0x7fffffff, 0x7fffffff + 2]);

const uint32_or = (a, b) => (a >>> b) || 7;
test(uint32_or, [7, 0, 0], [3, 3, 0], [0xffffffff, -1, 0], [0x7fffffff, -1, 1]);

// Smi-range Int32 (UnsafeSmiTagInt32) and IntPtr (IntPtrToNumber).
const smi_or = s => s.length || 'empty';
test(smi_or, ['empty', ''], [3, 'abc']);

const intptr_or = ta => ta.length || -1;
test(intptr_or, [-1, new Uint8Array(0)], [5, new Uint8Array(5)]);

// Float64 returned from a non-eagerly-inlined call (ReturnedValue).
const round4 = x => Math.round(x / 4);
%DisableEagerInlining(round4);
function returned_if(x, cond) {
  let v = round4(x);
  if (cond) v = 32;
  return v;
}
test(returned_if, [32, 0.5, true], [-0, -0.5, false], [NaN, NaN, false],
     [3, 12, false], [2.5e10, 1e11, false]);

// HoleyFloat64 over `||` and `&&` (preserving hole->undefined, -0, and NaN).
const holey = [1.5, , -0, NaN, 0, 2 ** 40];
const holey_float64_or = i => holey[i] || 9;
test(holey_float64_or, [1.5, 0], [9, 1], [9, 2], [9, 3], [9, 4], [2 ** 40, 5]);

const holey_float64_and = i => holey[i] && 9;
test(holey_float64_and,
     [9, 0], [undefined, 1], [-0, 2], [NaN, 3], [0, 4], [9, 5]);

// Tagging with another tagged use must not sink.
const box = {v: 'x'};
function float64_or_with_other_use(x) {
  const r = Math.round(x / 4);
  box.v = r;
  return r || 32;
}
test(float64_or_with_other_use, [32, 0.5], [32, -0.5], [3, 12], [2.5e10, 1e11]);
assertSame(2.5e10, box.v);

// Two values merged over the same edge.
function two_values(x, y, pick) {
  let a = x * 0.5, b = y * 0.25;
  if (a > b) {
    a = 1;
    b = 2;
  }
  return pick ? a : b;
}
test(two_values, [1, 10, 4, true], [2, 10, 4, false], [0.5, 1, 6, true],
     [1.5, 1, 6, false], [NaN, NaN, 4, true]);

// Inside a loop.
function in_loop(arr) {
  let last;
  for (let i = 0; i < arr.length; i++) last = Math.round(arr[i] / 4) || 32;
  return last;
}
test(in_loop, [32, [0.5, -0.5, NaN]], [2.5e10, [0.5, 1e11]], [-3, [1e11, -12]]);
