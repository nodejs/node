// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const o0 = {
};
function f1(a2, a3, a4) {
    function f5(a6) {
        const v13 = Math.min(Math.max(a3, 0), 2 & 52);
        o0.valueOf = f1;
        return v13 + v13;
    }
    %PrepareFunctionForOptimization(f5);
    f5(o0);
    const v16 = %OptimizeFunctionOnNextCall(f5);
    return v16;
}
for (let v17 = 0; v17 < 5; v17++) {
    f1(v17, v17);
}
