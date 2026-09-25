// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

function f0() {
    try {
        undefined.x;
    } catch(e4) {
        gc();
        const o7 = {
            "preventExtensions": e4,
        };
    }
}

%PrepareFunctionForOptimization(f0);
f0();
%OptimizeFunctionOnNextCall(f0);
f0();
