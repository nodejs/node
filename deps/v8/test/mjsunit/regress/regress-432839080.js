// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function f0() {
    const v1 = [];
    v1[8] = 4.2;
    for (let v3 = 0; v3 < 5; v3++) {
        let v4 = v1[4];
        for (let v5 = 0; v5 < 5; v5++) {
            v3 = v4;
            try { v4.n(); } catch (e) {}
            ++v4;
        }
    }
}

%PrepareFunctionForOptimization(f0);
f0();
f0();
%OptimizeFunctionOnNextCall(f0);
f0();
