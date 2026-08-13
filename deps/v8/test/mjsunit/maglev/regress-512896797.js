// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fuzzing --jit-fuzzing

let __v_1;
let __v_2 = {};
try {
    while(true) {
        try {
            __v_1 = 1;
        } catch(e) {
            break;
        } finally {
            with({}) throw Error();
        }
    };
} catch(e) {}
for (let i = 0; i < 4000; i++) {
    __v_2.a = 1;
}
