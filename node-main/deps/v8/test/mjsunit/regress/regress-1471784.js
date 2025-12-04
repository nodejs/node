// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:

for (let i = 0; i < 5; i++) {
    function f0() {
        const v3 = String();
        function f4(a5, a6, a7, a8) {
            a5++;
            const v10 = a5 / a5;
            const v11 = a8.bind();
            const t7 = v11.constructor;
            t7();
            try { v14 = v11.apply(v10, -2n); } catch (e) {}
        }
        for (let v15 = 0; v15 < 10; v15++) {
            f4(v15, v15, v3, f4);
        }
    }
    f0();
}
