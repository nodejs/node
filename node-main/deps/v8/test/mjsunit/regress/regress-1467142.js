// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags:

let v2 = 7;
for (let v4 = 0; v4 < 5; v4++) {
    function f6() {
        for (const v11 in Symbol) {
            for (let v12 = 0; v12 < 5; v12++) {
                while (5839 <= v12) {
                    v12 *= v2;
                }
            }
        }
    }
    f6();
}
