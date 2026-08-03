// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --noconcurrent-osr --turbolev
// Flags: --nolazy-feedback-allocation

function f6(a10) {
    const v11 = %WasmStruct();
    v11[a10];
    return v11;
}
function outer() {
  for (let i = 0; i < 10; i++) {
    f6(4);
  }
  for (let i = 0; i < 10; i++) {
    f6("string");
    if (i == 2) %OptimizeOsr(0);
  }
}
%PrepareFunctionForOptimization(outer);
outer();

// Regression test for issue 422320283, which was very similar:

function f0() {
  const v3 = %WasmStruct();
  function f5(v3, a6) {
    v3[a6];
  }
  f5(v3, "a");
  f5(v3, "b");
}
%PrepareFunctionForOptimization(f0);
f0();
f0();
%OptimizeFunctionOnNextCall(f0);
f0();

// Issue 424245314: KeyedHasIC should not misclassify its feedback.

function f4() {
    const v9 = %WasmArray();
    return 8 in v9;
}
%PrepareFunctionForOptimization(f4);
f4();
f4();
%OptimizeFunctionOnNextCall(f4);
f4();
