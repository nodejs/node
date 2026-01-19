// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

var builder = new WasmModuleBuilder();

let callee = builder.addFunction('callee', kSig_i_v).addBody([
    kExprI32Const, 1,
]);

let kLastLocalIndex = 600;
let kNumLocals = kLastLocalIndex + 1;
let kDelta = 10;
let kExpectedResult = kLastLocalIndex + kDelta;

function MakeBody(variant) {
    let body = [];

    // Initialize all locals with constants.
    for (let i = 0; i <= kLastLocalIndex; i++) {
        body.push(...wasmI32Const(i), kExprLocalSet, ...wasmUnsignedLeb(i));
    }

    // Last local += kDelta (causes it to be held in a register).
    body.push(
        kExprLocalGet, ...wasmUnsignedLeb(kLastLocalIndex),
        kExprI32Const, kDelta,
        kExprI32Add,
        kExprLocalSet, ...wasmUnsignedLeb(kLastLocalIndex));

    // If (false) { call callee; }
    body.push(
        kExprLocalGet, ...wasmUnsignedLeb(kLastLocalIndex),
        ...wasmI32Const(kLastLocalIndex + 2 * kDelta),
        kExprI32GtU,
        kExprIf, kWasmVoid,
        kExprCallFunction, callee.index);
    if (variant == 0) {
        body.push(kExprDrop);
    } else if (variant == 1) {
        // Early control flow edges to the {end} take a different code path.
        body.push(kExprBrIf, 0);
    }
    body.push(kExprEnd);

    body.push(kExprLocalGet, ...wasmUnsignedLeb(kLastLocalIndex));
    return body;
}

builder.addFunction('test1', kSig_i_v)
    .exportFunc()
    .addLocals(kWasmI32, kNumLocals)
    .addBody(MakeBody(0));
builder.addFunction('test2', kSig_i_v)
    .exportFunc()
    .addLocals(kWasmI32, kNumLocals)
    .addBody(MakeBody(1));

let instance = builder.instantiate();
assertEquals(kExpectedResult, instance.exports.test1());
assertEquals(kExpectedResult, instance.exports.test2());
