// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f1(a2) {
    let v4 = -0;
    if (a2) {
        v4 = -3;
    }
    return (v4 ** 69) != -2147483648;
}
const v11 = %PrepareFunctionForOptimization(f1);
f1();
const v13 = %OptimizeFunctionOnNextCall(f1);
f1(f1);
