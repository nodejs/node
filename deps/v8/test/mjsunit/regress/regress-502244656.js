// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fuzzing --jit-fuzzing

const v1 = new Set();
function f2(a3, a4, a5) {
    const v10 = {
        get c() {
            eval();
            v1?.__proto__;
            return v1;
        },
    };
    return a5;
}
f2();
