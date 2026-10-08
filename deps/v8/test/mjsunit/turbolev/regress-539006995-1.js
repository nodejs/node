// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation --maglev-non-eager-inlining --allow-natives-syntax --fuzzing

const v2 = Object.getPrototypeOf;
const v3 = Object.setPrototypeOf;
function f4(a5, a6) {
    const v7 = {};
    const v9 = new Proxy(v2(a6), v7);
    return v3(a6, v9);
}
const v11 = { probe: f4 };
for (let v12 = 0; v12 < 5; v12++) {
    function f14(a15, a16, a17, ...a18) {
        v11.probe("v11", a18);
        return a18.sort(f14);
    }
    f14().includes(v12, v12);
    const v24 = %OptimizeOsr();
}
