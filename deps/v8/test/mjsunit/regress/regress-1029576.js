// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  try {
    return BigInt.asUintN(8);
  } catch(_) {
    return 42n;
  }
}

%PrepareFunctionForOptimization(f);
assertEquals(42n, f());
assertEquals(42n, f());
%OptimizeFunctionOnNextCall(f);
assertEquals(42n, f());
