// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f1(a2) {
    const v4 = Infinity ? "23446" : Infinity;
    let v5 = 0;
    v5++;
    const v9 = Math.min(v4, !v5);
    let v10 = 0;
    v10--;
    let v12 = v9 % v10;
    if (a2) {
        v12 = 1.3;
    }
    const v16 = v12 ? 1 : 0;
    v16 / v16;
    return f1;
}
%PrepareFunctionForOptimization(f1);
f1();
%OptimizeFunctionOnNextCall(f1);
f1();
