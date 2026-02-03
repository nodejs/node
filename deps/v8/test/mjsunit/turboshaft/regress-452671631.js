// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function f0() {
    async function f1(a2, a3, a4, a5) {
        for (let v6 = 0; v6 < 5; v6++) {
            for (let [i11, i12] = (() => {
                    const v10 = new Int32Array();
                    v10[0] = v10;
                    return [0, 10];
                })();
                i11;
                ) {
                await using v17 = v6;
            }
            for (let i20 = 0, i21 = 10; i21; i21--) {
            }
            function f27(a28, a29, a30, a31) {
                return a31;
            }
            f27.d = f1;
            f27.d = f27;
        }
        return a3;
    }
    %PrepareFunctionForOptimization(f1);
    f1.call(f0, f1, f0);
    %OptimizeFunctionOnNextCall(f1);
    return f0;
}

f0();
const t26 = f0();
t26();
