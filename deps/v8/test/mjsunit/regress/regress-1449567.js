// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:

const v1 = new Uint8ClampedArray();
for (let v2 = 0; v2 < 2; v2++) {
    let v4 = 1024;
    function f6() {
        return f6;
    }
    const v7 = v4 >>> 0;
    for (let v8 = 0; v8 < 5000; v8++) {
        const v13 = v7 / 1024;
        const v14 = new Uint8Array(255);
        for (const v15 of v14) {
            -Math.floor(v13);
        }
    }
    for (const v18 of v1) {
    }
}
