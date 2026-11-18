// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function add(a, b) {
  return a + b;
}

function foo(arr, i, flag) {
  let c = (flag & 1) < 2;
  let x = arr[i];
  let p = c ? x : "str";
  let s = p + 0;
  let zero = c ? -0.0 : 1.5;
  return s + add(p, zero);
}

let arr = [1.5, 2.0, 3.5];
arr[5] = 4.0;

%PrepareFunctionForOptimization(add);
%PrepareFunctionForOptimization(foo);
add(1.5, 2.5);
foo(arr, 1, 0);

%OptimizeFunctionOnNextCall(foo);
foo(arr, 1, 0);
