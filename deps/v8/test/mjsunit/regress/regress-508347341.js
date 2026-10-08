// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-fast-api

const v1 = d8.test;
const t1 = v1.FastCAPI;
const v3 = new t1();
function f4(a5) {
    const v7 = a5 - 5;
    let v8 = 0;
    const v9 = v8--;
    return v3.sum_uint64_as_number(v7, v8);
}
const v11 = %PrepareFunctionForOptimization(f4);
for (let v12 = 0; v12 < 5; v12++) {
    try {
        const v14 = f4(v12);
    } catch(e15) {
    }
}
const v16 = %OptimizeFunctionOnNextCall(f4);
assertThrows(() =>f4(v1));
