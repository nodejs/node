// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

async function foo(cond) {
  while (cond) {
    cond.a;
    while (cond) {
      await 1;
      cond.b;
    }
  }
}

%PrepareFunctionForOptimization(foo);
foo(false);
%OptimizeFunctionOnNextCall(foo, "concurrent");
foo(false);
%FinalizeOptimization();
