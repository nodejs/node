// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --jit-fuzzing

for (let v0 = 0; v0 < 25; v0++) {
    async function f1() {
        for (let i7 = 4294967296;
            i7 < 6;
            (() => {
                i7++;
            })()) {
            await a4;
        }
    }
    f1();
}
