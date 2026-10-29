// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(a1, a2) {
  a1 = a1 | 0;
  const v9 = a2 | 8;
  const v10 = {};
  function f11() {
    for (let i13 = 0; v9;) {
    }
    return v10;
  }
  return 0;
}

function foo(a17, a18) {
  let v19 = 0;
  for (let i = 0; i < 5; i++) {
    v19 = bar() & a18;
  }
  return v19 | 0;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo();

%OptimizeFunctionOnNextCall(foo);
foo();
