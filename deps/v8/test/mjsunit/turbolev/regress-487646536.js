// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-future --maglev-assert-types

function foo(b) {
  let a;
  try {
    if (b) {
      (0).lastIndexOf(); // Always throws
      a = 4294443007; // n12
    } else {
      a = 3.5; // n14
    }
    // n17 = φᵀ r0 (n12, n14)
  } catch (e) {} // n5 = undefined; if b == true we end up here
  // n40 = φᵀ r0 (n16, n5)
  a | 0; // Int32 use
  a ?? 1337; // BranchIfUndefinedOrNull
}
%PrepareFunctionForOptimization(foo);
foo(true);

%OptimizeFunctionOnNextCall(foo);
foo(false);
