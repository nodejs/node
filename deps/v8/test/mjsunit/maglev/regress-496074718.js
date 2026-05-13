// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --fuzzing   --turboshaft-assert-types

const v1 = Date(Date);
let v2 = v1.setFullYear;
for (let v4 = 0; v4 < 5; v4++) {
    %OptimizeOsr();
    try {
        let v5;
        try { v5 = DataView.constructor(v1); } catch (e) {}
        function f7(a8, a9) {
            const v15 = {
                [v5](a11, a12, a13, ...a14) {
                },
            };
            return f7;
        }
        for (let i17 = 10, i18 = 10; i18; i18--) {
        }
        Map(DataView);
    } catch(e25) {
        v2++;
    }
}
