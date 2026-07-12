// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(arr) { arr.pop(); }
%NeverOptimizeFunction(bar);

function foo(x) {
  let arr = [1, x, 3];
  bar(arr);
  arr[2] = 42;
  return arr;
}

%PrepareFunctionForOptimization(foo);
assertEquals([1,undefined,42], foo());
assertEquals([1,undefined,42], foo());

%OptimizeMaglevOnNextCall(foo);
assertEquals([1,undefined,42], foo());
