// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f0() {
    for (let v1 = 0; v1 < 5; v1++) {
        const v3 = this.Math;
        v1 >>> v1;
        const v5 = v3.abs;
        function f6(a7, a8) {
            a7 ?? a7;
            0 + v5(a8 ?? a8);
            return v5;
        }
        f6();
        f6(1073741824, 2147483648);
    }
    return f0;
}

%PrepareFunctionForOptimization(f0);
f0();
%OptimizeFunctionOnNextCall(f0);
f0();
