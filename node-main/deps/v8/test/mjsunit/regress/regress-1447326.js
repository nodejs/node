// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

function F1() {
    if (!new.target) { throw 'must be called with new'; }
    this.a = 2070533028;
    this.f = 2070533028;
}
const v4 = new F1();
function f5(a6) {
    a6.f = 1854;
    a6.f;
    return 1854;
}
const v8 = f5(v4);
function test() {
    const o12 = {
        "execution": "async",
    };
    const v13 = gc(o12);
    const t17 = v13.finally(1854, v13, v13).constructor;
    new t17(f5);
    for (let i = 0; i < 5; i++) {
    }
    return v8;
}

%PrepareFunctionForOptimization(test);
test();
%OptimizeFunctionOnNextCall(test);
test();
