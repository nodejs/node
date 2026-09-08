// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function foo(a, b) {
  return a % b;
}
%PrepareFunctionForOptimization(foo);

const left = 261387672286309216;
const right = 936873377370284;

const result = foo(left, right);
assertEquals(936873377370264, result);

%OptimizeFunctionOnNextCall(foo);

const resultOpt = foo(left, right);
assertEquals(936873377370264, resultOpt);
