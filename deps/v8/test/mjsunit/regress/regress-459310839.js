// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
    for (let v0 = 0; v0 < 5; v0++) {
        const v1 = %OptimizeOsr();
        function f2() {
            let v3 = 0;
            const v5 = Math.expm1(v3);
            v5 * v5;
            v3 ^= v5;
            return Math;
        }
        f2();
    }
}

%PrepareFunctionForOptimization(foo);
foo();
