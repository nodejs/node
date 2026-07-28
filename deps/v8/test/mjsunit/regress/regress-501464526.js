// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fuzzing --jit-fuzzing

const v0 = [];
function f1(a2) {
    try {
    } catch(e3) {
        function f4() {
            return v0;
        }
    }
    return f1;
}
f1()();
