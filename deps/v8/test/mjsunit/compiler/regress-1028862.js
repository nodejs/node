// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  for (let i = 0; i < 5; i++) {
    // Only allocate feedback vector after the first round so that we get Smi
    // feedback for the modulus operation.
    if (i == 1) %PrepareFunctionForOptimization(foo);
    1 == new Date(42).getMilliseconds() % i;
  }
}

foo();
%OptimizeFunctionOnNextCall(foo);
foo();
