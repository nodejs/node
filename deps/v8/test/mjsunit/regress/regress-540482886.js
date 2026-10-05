// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// An operand of a Word32 truncated additive safe integer operation is only
// range checked when it needs a representation change. These operands are
// already Word32 or Word64, so the check disappears and exact Word32
// arithmetic diverges from the rounded Number semantics.

const mask = 0x5A82799;  // mask * mask is just below 2^53.

// The result of the inner addition reaches 2^53, twice the range its operands
// are checked against.
function smallIntegerAdd(x, c) {
  const m = x & 0x3ffffff;
  const p = m * m;
  return ((p + p) + c) | 0;
}

// The multiplication is truncated to Int32Mul, and its type only bounds it by
// the safe integer range.
function multiply(x, c) {
  const m = x & mask;
  return ((m * m) + c) | 0;
}

// Math.max is lowered to Int64Max, and the Word64 to Word32 truncation used to
// drop the check as well.
function word64(x, c) {
  const m = x & mask;
  return (Math.max(m * m, 0) + c) | 0;
}

%PrepareFunctionForOptimization(smallIntegerAdd);
for (let i = 0; i < 5; i++) {
  smallIntegerAdd(1, 0x2000000000000);
  smallIntegerAdd(2, 3);
}
%OptimizeFunctionOnNextCall(smallIntegerAdd);
assertEquals(0, smallIntegerAdd(0x3ffffff, 0xFFFFFFF));

%PrepareFunctionForOptimization(multiply);
for (let i = 0; i < 5; i++) {
  multiply(1, 0x2000000000000);
  multiply(2, 3);
}
%OptimizeFunctionOnNextCall(multiply);
assertEquals(15726960, multiply(mask, 0x8000000));

%PrepareFunctionForOptimization(word64);
for (let i = 0; i < 5; i++) {
  word64(0x1000000, 3);
  word64(0x1000000, 5);
}
%OptimizeFunctionOnNextCall(word64);
assertEquals(15726960, word64(mask, 0x8000000));
