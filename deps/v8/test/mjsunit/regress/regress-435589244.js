// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turboshaft-assert-types --jit-fuzzing
function f0() {
    function f1(a2) {
        let v3 = 3.1;
        for (let v4 = 0; v4 < 100; v4++) {
        }
        if (!a2) {
            v3 = -1584028104;
        }
        return v3 / Infinity;
    }
    f1(f1);
    const v9 = f1(f0);
    f1();
    eval();
    return v9;
}
f0.call();
