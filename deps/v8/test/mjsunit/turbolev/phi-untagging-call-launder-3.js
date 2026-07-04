// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap --turbolev

function bar(c) {
  c+c+c+c+c+c+c+c+c+c+c+c; // preventing eager inlining.
  return c ? 0x7ffffff1 : 42;
}

function foo(arr, c) {
  let x = bar(c);
  x + 2;
  arr.push(x);
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo([], false);

%OptimizeMaglevOnNextCall(foo);
foo([], true);
