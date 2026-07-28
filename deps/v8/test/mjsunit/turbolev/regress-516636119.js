// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-untagged-phis
// Flags: --no-lazy-feedback-allocation --single-threaded
// Flags: --invocation-count-for-osr=1 --invocation-count-for-turbofan=1

function f0() {
    return f0;
}
function main() {
    for (let v1 = 0; v1 < 5; v1++) {
        %OptimizeOsr();
        function f3() {
            let v4 = 2147483648;
            const v5 = v4--;
            v5 - v5;
            if (v1 === "boolean") {
                throw v4;
            }
            return v4;
        }
        const v10 = f3();
        function f11(a12, a13, a14, a15) {
            a13();
            a13();
            return a15;
        }
        f11(f0, f3);
        try { f11(f3, v10); } catch (e) {}
    }
}
%PrepareFunctionForOptimization(main);
main();
