// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function f1() {
    return 615.5720999180407;
}
class C2 extends f1 {
}
const v3 = new C2();
Object.defineProperty(v3, "length", { value: 615.5720999180407 });
const v5 = ("o").normalize();
function F7(a9, a10, a11) {
    if (!new.target) { throw 'must be called with new'; }
    try { new a10(v3); } catch (e) {}
    a9.length;
}
%PrepareFunctionForOptimization(F7);
new F7(v5, F7);
%OptimizeMaglevOnNextCall(F7);
new F7(v5, F7);
