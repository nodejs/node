// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let v2 = new BigUint64Array(256);
for (let v3 = 0; v3 < 5; v3++) {
    let v4 = -2147483647;
    do {
        for (let v6 = 0; v6 < 5; v6++) {
        }
        let v8 = v2++;
        v8--;
        v8 % v8;
    } while (v4 >= 1)
}
