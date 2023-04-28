// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

async function f(a, b) {
  let x = 0xfffffffff;
  if (b == 5) {
    x = 0xffffffff1;
  }
  let y = Math.max(0xffffffff2, x);
  return BigInt.asUintN(1, y);
};

%PrepareFunctionForOptimization(f);
assertThrowsAsync(f(1, 2), TypeError);
%OptimizeFunctionOnNextCall(f);
assertThrowsAsync(f(1, 2), TypeError);
if (%Is64Bit()) assertUnoptimized(f);
%PrepareFunctionForOptimization(f);
assertThrowsAsync(f(1, 2), TypeError);
%OptimizeFunctionOnNextCall(f);
assertThrowsAsync(f(1, 2), TypeError);
assertOptimized(f);
