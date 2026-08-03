// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --invocation-count-for-maglev=4 --single-threaded --verify-heap

for (let v0 = 0; v0 < 10; v0++) {
    function f1(a2) {
        const v4 = 1 & a2;
        const v6 = Math.floor(v0);
        v8 =undefined / undefined | 0;
        const v13 = Array();
        v13[521] = v8;
        for (let i15 = 0; i15 < v4; i15++) {
            i15 + v6;
            v13[i15] = i15;
            i15 = f1;
            i15++;
        }
        v13.sort(f1);
    }
    f1("5001");
}
