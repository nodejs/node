// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --single-threaded

for (let v1 = 0; v1 < 1000; v1++) {
    v1++;
    function f3() {
    }
    class C4 extends f3 {
        constructor(a6) {
            return a6;
        }
    }
    try { new C4(v1); } catch (e) {}
}
