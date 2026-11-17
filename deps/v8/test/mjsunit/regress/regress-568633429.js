// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --allow-natives-syntax

function outer() {
  try {
    throw 1;
  } catch (e) {}

  function inner(arg1, arg2) {
    for (let i = 0; i < 5; i++) {
      Number.isNaN(Math.log10(arg2), inner);
      arg2 = arg1;
    }
  }
  inner(inner, inner);
}

%PrepareFunctionForOptimization(outer);
outer();
outer();
%OptimizeFunctionOnNextCall(outer);
outer();
