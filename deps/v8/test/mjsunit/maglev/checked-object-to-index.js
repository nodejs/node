// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar() {
  return 3;
}

function foo() {
  let arr = [0, 2, 4, 8, 16, 32];
  let index = bar();
  return arr[index];
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
assertEquals(8, foo());

%OptimizeFunctionOnNextCall(foo);
assertEquals(8, foo());
