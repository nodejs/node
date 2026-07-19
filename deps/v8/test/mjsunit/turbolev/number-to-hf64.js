// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function bar(c) {
  c + c + c + c + c + c + c + c + c + c + c; // preventing eager inlining
  return c + 1.5;
}

function foo(arr, c) {
  arr[0] = bar(c);
}

let hf64_arr = [ 1.5, 2.5, /*hole*/, 4.5];

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo(hf64_arr, 1);
assertEquals(hf64_arr[0], 2.5);

hf64_arr[0] = 1.5;

%OptimizeFunctionOnNextCall(foo);
foo(hf64_arr, 1);
assertEquals(hf64_arr[0], 2.5);
