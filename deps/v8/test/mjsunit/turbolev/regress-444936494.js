// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax  --no-lazy-feedback-allocation

function f1() {
    var DONT_DELETE = 4;
    function f5(a6, a7, a8, a9) {
        const v12 = (a9 & DONT_DELETE) === 0;
        const v13 = { configurable: v12 };
    }
    function f14() {
    }
    return f5({}, "foo", f14, -1);
}
%PrepareFunctionForOptimization(f1);
f1();
f1();
%OptimizeFunctionOnNextCall(f1);
f1();
