// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f0(a1, a2) {
    for (let i = 0; i < 5; i++) {
        const v3 = a1 | a2;
        for (let v4 = 0; v4 < 5; v4++) {
        }
        a1 = v3;
    }
    return a1;
}
function f5() {
    let v7 = f0(Infinity);
    return v7--;
}

%PrepareFunctionForOptimization(f0);
%PrepareFunctionForOptimization(f5);
f5();
%OptimizeFunctionOnNextCall(f5);
f5();
