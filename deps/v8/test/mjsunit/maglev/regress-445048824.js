// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax  --no-lazy-feedback-allocation

function f0() {
    function f2(a3) {
        for (let v5 = 0; v5 < 5; v5++) {
            try { a3.call(); } catch (e) {}
        }
    }
    function f7(a8) {
        Array.prototype.slice.call(arguments);
        function f14() {
            Array.slice.call();
        }
        return f14;
    }
    const t15 = Function.prototype;
    t15.bind = f7;
    function f19() {
    }
    f2(f19.bind());
}

%PrepareFunctionForOptimization(f0);
f0();
%OptimizeFunctionOnNextCall(f0);
f0();
