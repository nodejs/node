// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

function f(a, b, c) {
  // CheckBigInt64 is required if the type of input is UnsignedBigInt64
  // because its value can be out of the range of SignedBigInt64.
  let t = BigInt.asUintN(64, a + b);
  // The addition is speculated as CheckedInt64Add and triggers the deopt
  // for the large value coming in through <t>.
  return t + c;
}

%PrepareFunctionForOptimization(f);
assertEquals(12n, f(9n, 2n, 1n));
%OptimizeFunctionOnNextCall(f);
assertEquals(12n, f(9n, 2n, 1n));
assertOptimized(f);
assertEquals(2n ** 64n, f(1n, -2n, 1n));
if (%Is64Bit()) {
  assertUnoptimized(f);
}
