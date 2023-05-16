// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar() {
  arr = new Array(4);
  iter = arr[Symbol.iterator];
  return iter;
}

function foo(a) {
  iter = bar();
  return iter.isPrototypeOf(iter);
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
