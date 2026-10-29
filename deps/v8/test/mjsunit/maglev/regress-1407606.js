// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

for (let v0 = 0; v0 < 100; v0++) {
    for (let v1 = 0; v1 < 100; v1++) {
        const v4 = new Float64Array(33519);
    }
    for (let v5 = 0; v5 < 100; v5++) {
        function F8(  a12) {
            if (!new.target) { throw 'must be called with new'; }
            a12--;
        }
        const v14 = new F8(- -1000000.0);
    }
}
