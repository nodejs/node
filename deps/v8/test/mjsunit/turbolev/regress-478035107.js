// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function f2() {
    var phi1 = 1.1;
    let v4 = 1.1 << 0.5;
    for (let v5 = 0; v5 < 5; v5++) {
        1.1 >> v4;
        var setUTCMilliseconds = phi1;
        setUTCMilliseconds != setUTCMilliseconds;
        phi1 = v4;
        v4 = setUTCMilliseconds;
    }
    return phi1;
}
function f10() {
    return f2();
}
%PrepareFunctionForOptimization(f2);
f10();
%OptimizeFunctionOnNextCall(f2);
f10();
