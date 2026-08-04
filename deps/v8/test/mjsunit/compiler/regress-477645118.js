// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

Int32Array.toString = Array;

function foo() {
  for (let i = 0; i < 5; i++) {
    const arr = [12];
    arr.a = Int32Array;
    function bar(x) {
      x.a = x;
      return arr;
    }
    bar(Int32Array);
    bar(arr);
  }
  return foo;
}

%PrepareFunctionForOptimization(foo);
foo();
foo();

%OptimizeFunctionOnNextCall(foo);
foo();
