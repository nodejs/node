// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:

function f1() {
    return f1;
}
const v2 = f1();
function F3() {
    function f6() {
        let v7 = (10).length;
        v7++;
        Array(v7, 10, 10, v2);
        return this;
    }
    f6();
    for (let i=0; i<500; i++){
        f6();
    }
}
new F3();
new F3();
new F3();
