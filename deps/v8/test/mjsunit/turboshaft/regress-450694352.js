// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function f0() {
    return f0;
}
async function* f1(a2, a3) {
    const v4 = [f0];
    for (let v5 = 0; v5 < 5; v5++) {
        for (let i8 = 12, i9 = 10; i9; i9--) {
        }
        const v16 = { maxByteLength: 256 };
        for (let i18 = 0; 5;) {
            function f22() {
                return f22;
            }
            v4[v16.maxByteLength] /= a2;
            await v4;
            return f22;
        }
    }
    return v4;
}

%PrepareFunctionForOptimization(f1);
f1(f1, f1).next();

%OptimizeFunctionOnNextCall(f1);
