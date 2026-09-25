// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation
// Flags: --single-threaded --invocation-count-for-maglev=10

function f0() {
    class C1 {
        constructor(a3) {
            try { a3(); } catch (e) {}
            for (let i = 0; i < 25; i++) {
            }
        }
    }
    const v5 = new C1(C1);
    return v5;
}
f0();
const t11 = f0().constructor;
new t11();
