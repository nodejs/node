// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function f0() {
    function f4() {
        const v6 = class {
            p( a9, a10) {
                a9.length = a9;
                const t5 = a10?.p;
                t5();
            }
        }
        const v13 = new v6();
        try { v13.p(); } catch (e) {}
        v13.p( v13, v13);
    }
    try { f4.call(); } catch (e) {}
}
%PrepareFunctionForOptimization(f0);
f0.call();
f0();
%OptimizeFunctionOnNextCall(f0);
f0();
