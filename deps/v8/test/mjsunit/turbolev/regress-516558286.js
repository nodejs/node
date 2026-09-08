// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --no-lazy-feedback-allocation

let v0 = 0;
function f2(a3, a4) {
    return a3;
}
function f5(a6, a7) {
    if (v0 < 5) {
        const v11 = v0 + 1;
        v0 = v11;
        f2(1, v11);
    }
    function F14(a16) {
        if (!new.target) { throw 'must be called with new'; }
        gc();
        gc(-3.0);
        const v24 = {
            [Int16Array](a22, a23) {
            },
        };
        v24.e = a16;
        %PretenureAllocationSite(v24);
    }
    const v25 = new F14(a7);
    const v26 = new F14(v25);
    const t23 = v26.constructor;
    new t23(a7);
    SharedArrayBuffer();
    return f5;
}
f2 = f5;
%PrepareFunctionForOptimization(f5);
try { f5(f5, f5); } catch (e) {}
%OptimizeFunctionOnNextCall(f5);
try { f5(f2, f5); } catch (e) {}
