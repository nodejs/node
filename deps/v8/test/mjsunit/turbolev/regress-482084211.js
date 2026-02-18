// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f0() {
    for (let v1 = 0; v1 < 2; v1++) {
        function f1(a1) {
            function F2() {}
            function f3(a2, a3) {
                for (let v2 = 0; v2 < 3; v2++) {
                    if (v2) a1 + F2.id;
                }
                return a3;
            }
            f3(F2, f3);
        }
        f1(f1());
    }
}
for (let i = 0; i < 1000; i++) f0();
