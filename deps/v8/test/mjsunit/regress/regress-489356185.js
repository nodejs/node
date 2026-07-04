// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev --jit-fuzzing

function f0() {
    function f3(...a4) {
        function f5(a6) {
            const v8 = { params: a4, __proto__: null };
            var start = "";
            function f11() {
                start.__proto__ = "";
            }
            return v8;
        }
        f5.call(a4, f5);
        return f3;
    }
    const v13 = f3();
    f3(    v13());
}
const v16 = %PrepareFunctionForOptimization(f0);
f0();
const v18 = %OptimizeFunctionOnNextCall(f0);
f0();
