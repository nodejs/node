// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
    const v0 = [];
    v0[8] = 4.2;
    for (let v2 = 0; v2 < 5; v2++) {
        v2++;
        const v5 = v0[4] ?? v2;
        v5 ^ v5;
        const v7 = %OptimizeOsr();
    }
}

%PrepareFunctionForOptimization(foo);
foo();
