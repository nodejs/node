// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar() {
  const x = 0;
  return BigInt(x * -1);
}

%PrepareFunctionForOptimization(bar);
assertEquals(0n, bar());
%OptimizeFunctionOnNextCall(bar);
assertEquals(0n, bar());

function foo() {
  let result = 0n;
  let obj = {i: 0};
  for (; obj.i < 1; ++obj.i) {
    result += BigInt(obj.i * -2);
  }
  return result;
}

%PrepareFunctionForOptimization(foo);
assertEquals(0n, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(0n, foo());
