// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --fuzzing --no-lazy-feedback-allocation

function f0(a1, a2) {
    let newPrototype;
    const v11 = {
        set(a6, a7, a8, a9) {
            a9 === newPrototype;
            return newPrototype;
        },
    };
    try {
    } catch(e12) {
    }
    return undefined;
}
function f13(a14, a15) {
    f0();
    return f13;
}
const v17 = { probe: f13 };
for (let v18 = 0; v18 < 5; v18++) {
    const v38 = v18;
    const v21 = %OptimizeOsr();
    const v22 = %WasmArray();
    function f23(a24, a25, a26, a27) {
        v17.probe("v48", v38 | a26);
        return v38;
    }
    f23(v22, 3767, 3767);
    ([]).forEach(f23);
}
