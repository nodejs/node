// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turboshaft --no-lazy --jit-fuzzing

for (let i = 0; i < 100; i++) {
    function f() {
    }
    const a = Array(2);
    try {
        a.reduceRight(f);
    } catch(e) {
    }
}
