// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --maglev-inlining --jit-fuzzing

function f8() {
    parseInt("-1024");
    const v12 = %OptimizeOsr();
}
const v14 = new Uint16Array(952);
const v16 = new Int16Array(v14);
let v18 = Object;
%PrepareFunctionForOptimization(f8);
for (const v23 in v16) {
    const v24 = `
        f8(Uint16Array, v18);
    `;
    eval(v24);
}
