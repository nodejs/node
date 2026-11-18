// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-non-eager-inlining
// Flags: --max-maglev-eager-inlined-bytecode-size=0
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

function selectUnary(which) {
  if (which === 0) return Math.trunc;
  if (which === 1) return Math.abs;
  return x => x;
}

function selectBinary(which) {
  if (which === 0) return Math.imul;
  if (which === 1) return Object.is;
  return (x, y) => x + y;
}

function callUnary(fn, x) {
  return fn(x);
}

function callBinary(fn, x, y) {
  return fn(x, y);
}

function testPostInliningBuiltins(which, a, b) {
  const u = selectUnary(which);
  const bin = selectBinary(which);
  return [
    callUnary(u, a + b),
    callUnary(u, -0.0),
    callBinary(bin, a | 0, b | 0),
  ];
}

%PrepareFunctionForOptimization(selectUnary);
%PrepareFunctionForOptimization(selectBinary);
%PrepareFunctionForOptimization(callUnary);
%PrepareFunctionForOptimization(callBinary);
%PrepareFunctionForOptimization(testPostInliningBuiltins);

for (let i = 0; i < 5; ++i) {
  assertEquals([3, -0.0, 2], testPostInliningBuiltins(0, 1.25, 2.5));
  assertEquals([3.75, 0.0, false], testPostInliningBuiltins(1, -1.25, -2.5));
  assertEquals([3.75, -0.0, 3], testPostInliningBuiltins(2, 1.25, 2.5));
}

%OptimizeFunctionOnNextCall(testPostInliningBuiltins);
assertEquals([3, -0.0, 2], testPostInliningBuiltins(0, 1.25, 2.5));
assertEquals([-3, -0.0, 2], testPostInliningBuiltins(0, -1.25, -2.5));
assertEquals([1, -0.0, -2], testPostInliningBuiltins(0, -1.25, 2.5));
assertEquals([3.75, 0.0, false], testPostInliningBuiltins(1, -1.25, -2.5));
assertEquals([0.0, 0.0, true], testPostInliningBuiltins(1, -0.0, 0.0));
assertEquals([3.75, -0.0, 3], testPostInliningBuiltins(2, 1.25, 2.5));
