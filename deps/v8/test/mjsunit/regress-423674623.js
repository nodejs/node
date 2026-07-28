// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --jit-fuzzing

function f0(a1) {
  function f2() {
      return Math.max((a1 | 0) * (2 ** 52), 4294967297);
  }
  let v12 = f2();
  return ~(v12-- + v12);
}
const v16 = %PrepareFunctionForOptimization(f0);
f0();
const v18 = %OptimizeFunctionOnNextCall(f0);
function f19() {
  f0(1);
  return v16;
}
const v22 = %PrepareFunctionForOptimization(f19);
f19();
const v24 = %OptimizeFunctionOnNextCall(f19);
f19();
