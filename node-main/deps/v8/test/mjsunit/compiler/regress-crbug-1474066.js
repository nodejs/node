// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboshaft --turboshaft-typed-optimizations

function f0() {
    for (let i3 = 0; i3 < 1; i3++) {
        function f9() {
        }
    }
}
const v16 = %PrepareFunctionForOptimization(f0);
const v18 = f0();
const v19 = %OptimizeFunctionOnNextCall(f0);
f0();
