// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar() {}
%NeverOptimizeFunction(bar);

function foo() {
  const arr = [/*hole*/, 1.5, 2.5, 3.5];
  let x = arr[0];

  try {
    bar(x);
    x -= 0;
    x.n();
  } catch (e) {}

  return x;
}

%PrepareFunctionForOptimization(foo);
assertEquals(NaN, foo());
assertEquals(NaN, foo());

%OptimizeFunctionOnNextCall(foo);
assertEquals(NaN, foo());
