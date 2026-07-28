// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const v3 = [0,,2.5];
function f4(a5) {
    const v6 = v3[1];
    for (let i8 = -0.0; i8 < a5;) {
        i8 += v6;
    }
    return v6;
}
%PrepareFunctionForOptimization(f4);
f4(5);
%OptimizeFunctionOnNextCall(f4);
f4();
