// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function F0() {
}
function f6(a7) {
    let v8 = 2147483647;
    if (a7) {
        v8 = -NaN;
    }
    let v11 = 1;
    v11--;
    assertTrue(isNaN(v8 / v11));
}
f6(F0);
%PrepareFunctionForOptimization(f6);
f6(F0);
%OptimizeFunctionOnNextCall(f6);
f6(F0);
