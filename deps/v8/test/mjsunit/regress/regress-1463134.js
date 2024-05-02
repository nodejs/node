// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const o0 = {
};
function F1() {
}
const v3 = new F1();
function f4(a5, a7) {
    a5.d = o0;
    function F8() {
    }
    const v12 = new F8();
    let v13 = 0;
    for (let i15 = ++v13; i15 <= v13; ++i15) {
        v12[3819725155] &= a5;
        function f20() {
        }
    }
    return a7;
}
f4(v3);
function f22() {
    const v24 = Array();
    v24.findLast(Array);
    v24.reduce(f4, 0);
}
const v28 = %PrepareFunctionForOptimization(f22);
f22();
const v30 = %OptimizeFunctionOnNextCall(f22);
f22();
