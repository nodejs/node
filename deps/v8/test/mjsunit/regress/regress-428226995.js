// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(opt) {
  class C {}
  function bar() {
    let x = 0;
    for (let i = 0; i < 5; i++) {
      C[2] = x;
      C[2] = x;
      x++;
    }
  }
  %PrepareFunctionForOptimization(bar);
  bar();
  bar();
  if (opt) {
    %OptimizeMaglevOnNextCall(bar);
  }
  bar();
}

foo(false);
foo(true)
