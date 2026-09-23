// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation --turbolev

const v0 = {};
let v1 = -9007199254740991;
let v2 = -60114;
v2++;
function f4(a5, a6, a7) {
    function f8(a9) {
        function f10(a11) {
            return a9;
        }
        f10(v0);
        f10(f8);
        v1 = a9;
        return a5;
    }
    f8(v2);
    f8(9007199254740991);
    return v1;
}
const v17 = f4();
f4();
%PrepareFunctionForOptimization(f4);
%OptimizeFunctionOnNextCall(f4);
f4(f4, v17, v1);
