// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(a6) {
    function f7() {
        try {
            return ("number").charCodeAt(0n);
        } catch(e11) {
        }
        function f12() {
            function f13(a14, a15) {
                eval();
                return f;
            }
            try { f13(); } catch (e) {}
            for (let i20 = 10; i20--, i20;) {
            }
            return a6;
        }
        return f12;
    }
    const t22 = f7();
    t22();
    return f;
}

%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f(f);
