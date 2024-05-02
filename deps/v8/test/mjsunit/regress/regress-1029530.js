// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --interrupt-budget=1024

{
  function f() {
    const b = BigInt.asUintN(4,3n);
    let i = 0;
    while(i < 1) {
      i + 1;
      i = b;
    }
  }

  %PrepareFunctionForOptimization(f);
  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
}


{
  function f() {
    const b = BigInt.asUintN(4,10n);
    let i = 0.1;
    while(i < 1.8) {
      i + 1;
      i = b;
    }
  }

  %PrepareFunctionForOptimization(f);
  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
}
