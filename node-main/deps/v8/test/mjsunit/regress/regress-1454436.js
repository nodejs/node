// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const v3 = new Uint8Array();
function f4() {
    for (let v5 = 0; v5 < 5; v5++) {
        v5++;
        const v7 = -v5;
        const v8 = v7 << v7;
        let v9 = v8 + v8;
        v9--;
        v9--;
        v3[undefined] &= v9;
    }
}

%PrepareFunctionForOptimization(f4);
f4();
%OptimizeFunctionOnNextCall(f4);
f4();
