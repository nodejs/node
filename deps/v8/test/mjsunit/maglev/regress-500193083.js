// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function outer() {
  let x = 0;
  let writer = function(v) { x = v; };

  %PrepareFunctionForOptimization(writer);

  function loopFunc() {
    x = 0;
    writer(1);

    if (x !== 1) return 1;

    %OptimizeMaglevOnNextCall(writer);
    return 0;
  }

  return loopFunc;
}

let f1 = outer();
let f2 = outer(); // Multiple closures.

f1();
%PrepareFunctionForOptimization(f1);
f1();
%OptimizeMaglevOnNextCall(f1);

assertEquals(0, f1());
