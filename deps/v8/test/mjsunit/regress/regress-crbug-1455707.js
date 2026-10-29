// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --jit-fuzzing

for (let v0 = 0; v0 < 5; v0++) {
    function f1() {
        return v0;
    }
    function f2() {
        return f2;
    }
    const v3 = f1.prototype;
    v3.f = f2;
    function f4() {
        [v3,v3,v3];
    }
    function f6() {
        function f7(a8, a9, a10) {
            const o11 = {
            };
            const o12 = {
            };
            o11.__proto__ = o12;
            const o13 = {
            };
            const t21 = o11.__proto__;
            t21.__proto__ = o13;
            for (let v15 = 0; v15 < 5; v15++) {
            }
            return a10;
        }
        f7(v3, f7, v0);
        return f4;
    }
    const t30 = f4.prototype;
    t30.f = f6;
    const v18 = new f1();
    const v19 = new f4();
    function f20(a21) {
        const t35 = a21.f();
        return t35();
    }
    %PrepareFunctionForOptimization(f20);
    f20(v18);
    f20(v19);
    const v26 = %OptimizeFunctionOnNextCall(f20);
}
