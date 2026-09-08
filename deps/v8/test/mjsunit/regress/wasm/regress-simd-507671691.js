// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $sig0 = builder.addType(kSig_i_iii);
let $mem0 = builder.addMemory(1);
let main = builder.addFunction('main', $sig0).exportFunc().addBody([
    kExprI32Const, 3,
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    ...wasmS128Const([56, 163, 217, 17, 212, 219, 128, 161, 109, 237, 215, 48, 210, 104, 247, 134]),
    kSimdPrefix, kExprI8x16Shuffle, 0, 14, 16, 8, 1, 8, 14, 7, 25, 17, 0, 17, 15, 24, 13, 8,
    kSimdPrefix, kExprI8x16Shuffle, 1, 8, 14, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    kSimdPrefix, kExprS128StoreMem, 1, ...wasmUnsignedLeb(2049),
    kExprI32Const, 1,
  ]);

const instance = builder.instantiate({});
instance.exports.main(1, 2, 3);
