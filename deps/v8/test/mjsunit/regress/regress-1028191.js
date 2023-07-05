// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

"use strict";

function f(a, b, c) {
  let x = BigInt.asUintN(64, a + b);
  try {
    x + c;
  } catch(_) {
    eval();
  }
  return x;
}

%PrepareFunctionForOptimization(f);
assertEquals(f(3n, 5n), 8n);
assertEquals(f(8n, 12n), 20n);
%OptimizeFunctionOnNextCall(f);
assertEquals(f(2n, 3n), 5n);
