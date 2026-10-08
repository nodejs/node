// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev-verify-dominance

function foo(x) {
  let phi = 0;
  for (let i = x; i; i--) {
    phi ^ 1;
    phi = i;
  }
}

%PrepareFunctionForOptimization(foo);
foo(10);

%OptimizeFunctionOnNextCall(foo);
foo(10);
