// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const v2 = new Int16Array(7);
const v5 = new Float32Array(40);
function f6(a7) {
    for (let v9 = 0; v9 < 5; v9++) {
        const v10 = %OptimizeOsr();
        function f11( a13) {
        }
    }
    const v14 = v2[3];
    const v15 = a7[15];
    const v16 = a7[5];
v14 + v15 ^ v16;
}
%PrepareFunctionForOptimization(f6);
f6(v5);
f6(v5);
const v21 = %OptimizeFunctionOnNextCall(f6);
try { f6(); } catch {}
