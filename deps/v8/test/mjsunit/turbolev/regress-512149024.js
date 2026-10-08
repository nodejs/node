// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function f0() {
    return f0;
}
function f1() {
    return f1;
}
function get_f1(a0, a1) {
    Math.floor(-a1);
    Function();
    return f1;
}
function opt_me() {
    for (let i = 0; i < 50; i++) {
        get_f1();
    }
    const f1 = get_f1();
    f1.call("AAA", f0);
}

%PrepareFunctionForOptimization(opt_me);
opt_me();
opt_me();
opt_me();
%OptimizeFunctionOnNextCall(opt_me);
opt_me();
