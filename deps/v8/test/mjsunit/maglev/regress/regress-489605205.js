// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --assert-types --jit-fuzzing

const v1 = {};
const v3 = new Float32Array();
function f4() {
    for (let v5 = 0; v5 < 50; v5++) {
        async function* f7() {
            try {
                class C8 {
                }
                function f9() {
                    Object.defineProperty(C8, 2279, { configurable: true, set: f4 });
                    v3[Symbol] = Symbol;
                    return Symbol;
                }
                f9();
                await Promise;
            } catch(e13) {
            }
        }
        f7().next(f4);
    }
    return v3;
}
f4.apply();
