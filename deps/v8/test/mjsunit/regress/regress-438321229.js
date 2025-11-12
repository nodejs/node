// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-fast-api --no-lazy-feedback-allocation

const t0 = d8.test.FastCAPI;
const v5 = new t0();
function f6(a7, a8) {
    a8.call_to_number(f6);
}
function f12() {
    function f14() {
        this.console.trace();
    }
    f14();
}
f6.valueOf = f12;
f6(2486, v5);
%PrepareFunctionForOptimization(f6);
%OptimizeFunctionOnNextCall(f6);
f6(1024, v5);
