// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let int32_context_cell = 42;
int32_context_cell = 0x40000000; // Making it non-smi but still int32

const obj = { x : 42 };

function g(x) {
  // Preventing eager inlining.
  let t = 0;
  t += 1; t += 2; t += 3; t += 4; t += 5;
  t += 6; t += 7; t += 8; t += 9; t += 10;
  t += 11; t += 12; t += 13; t += 14;

  // Introducing a CheckedNumberToInt32.
  int32_context_cell = x;

  return x;
}

function f(x) {
  const y = g(x);

  // Inserting a CheckedSmiUntag so that `obj.x = y` doesn't insert a CheckSmi.
  y | 0;

  obj.x = y;
}

%PrepareFunctionForOptimization(g);
%PrepareFunctionForOptimization(f);
f(17);

%OptimizeMaglevOnNextCall(f);
f(17);

f(%AllocateHeapNumberWithValue(42));
