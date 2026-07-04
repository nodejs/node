// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1, true);
builder.addFunction("main", makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]))
    .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 0,
        kSimdPrefix, ...wasmUnsignedLeb(kExprS128LoadMem), 0, 0,
        kExprLocalGet, 1,
        kSimdPrefix, ...wasmUnsignedLeb(kExprS128LoadMem), 0, 0,
        kSimdPrefix, ...wasmUnsignedLeb(kExprI8x16Shuffle),
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        kExprLocalGet, 2,
        kSimdPrefix, ...wasmUnsignedLeb(kExprS128LoadMem), 0, 0,
        kSimdPrefix, ...wasmUnsignedLeb(kExprI8x16Shuffle),
        8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23,
        kSimdPrefix, ...wasmUnsignedLeb(kExprI16x8SConvertI8x16Low),
        kSimdPrefix, ...wasmUnsignedLeb(kExprS128StoreMem), 0, 0,
        kExprLocalGet, 0
    ])
    .exportFunc();
const instance = builder.instantiate();
instance.exports.main(0, 16, 32);
