// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

// Regression test for 32-bit overflow-checked add/sub when the left operand
// has dirty upper bits (e.g. a zero-extended uint32 value combined with a
// 64-bit bitwise operation). If the overflow check assumes the operand is
// already sign-extended, the overflow deopt is skipped and the optimized code
// returns a wrapped int32 instead of the correct double.

var arr = new Uint32Array([0x80000000]);

// Immediate right operand.
function addImm(i) { return (arr[i] ^ -1) + 1; }

// Register right operand.
function addReg(i, d) { return (arr[i] ^ -1) + d; }

// Register right operand.
function subReg(i, d) { return (arr[i] ^ 1) - d; }

%PrepareFunctionForOptimization(addImm);
for (let k = 0; k < 200000; k++) {
  arr[0] = 0;
  addImm(0);
}
arr[0] = 0x80000000;
%OptimizeFunctionOnNextCall(addImm);
assertEquals(2147483648, addImm(0));
assertUnoptimized(addImm);

%PrepareFunctionForOptimization(addReg);
for (let k = 0; k < 200000; k++) {
  arr[0] = 0;
  addReg(0, 1);
}
arr[0] = 0x80000000;
%OptimizeFunctionOnNextCall(addReg);
assertEquals(2147483648, addReg(0, 1));
assertUnoptimized(addReg);

%PrepareFunctionForOptimization(subReg);
for (let k = 0; k < 200000; k++) {
  arr[0] = 0;
  subReg(0, 2);
}
arr[0] = 0x80000000;
%OptimizeFunctionOnNextCall(subReg);
assertEquals(-2147483649, subReg(0, 2));
assertUnoptimized(subReg);
