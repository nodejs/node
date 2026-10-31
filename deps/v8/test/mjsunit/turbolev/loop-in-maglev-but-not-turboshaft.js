// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function foo(x) {
  for (let i = 0; i < 10; ) {
    i++;
    let v1 = i << 2 | i >>> 28;
    let v2 = i >>> 28 | i << 2;
    // Turboshaft's MachineOptimizationReducer will optimize both {v1} and {v2}
    // to a simple rotation by 2, and GVN will merge them into a single
    // operation (all of this during TurbolevGraphBuilding). This means that
    // this loop in Maglev (that doesn't realize that {v1} and {v2} are the
    // same), but not in Turboshaft (that realizes that the `break` is always
    // taken).
    if (v1 == v2) break;
  }
}

%PrepareFunctionForOptimization(foo);
foo();
foo();

%OptimizeFunctionOnNextCall(foo);
foo();
