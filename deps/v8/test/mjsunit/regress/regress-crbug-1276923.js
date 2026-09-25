// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --gc-interval=10

// Base case where a GC observable store might be temporarily shadowed.
function foo() {
  let i = 0.1;
  eval();
  if (i) {
    const c = {};
    eval();
  }
}
%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();

// Stress execution with GCs.
function bar() {
  for (let cnt = 0, i = 655; cnt < 10000 && i !== 1; cnt++, i = i / 3) {
    i %= 2;
    const c = { "b": 1, "a":1, "c": 1,  "d": 1 };
    eval();
  }
}
bar();
