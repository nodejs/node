// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --expose-gc

let o1 = {f: 0};
gc();
gc({type: "minor"});
let mark_obj = {};
for (let i = 0; i < 70; i++) {
    o1[i] = {};
}
    new Array(0x4000);
function generateUafObj(mark_obj) {
    /*
    */
    let uaf = {};
    uaf.f = uaf;
    Object.assign(mark_obj, uaf);
}
%PrepareFunctionForOptimization(generateUafObj);
generateUafObj({});
%OptimizeFunctionOnNextCall(generateUafObj);
generateUafObj(mark_obj);
