// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --future-wasm-simd-opt

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $sig0 = builder.addType(kSig_i_iii);
let main = builder.addFunction('main', $sig0).exportFunc();

// Note that the comments describe the analysis which runs backwards, so the
// comments should be read backwards (starting with the extract lane). :)
main.addBody([
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    // Shuffle A.
    kSimdPrefix, kExprI8x16Shuffle, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    ...wasmS128Const([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]),
    // Shuffle B. As Shuffle C demands byte 4, this should read the '24' of this
    // shuffle mask, which maps to the constant 8 in the v128.const instruction.
    // However, due to the bug in the analsysis, it incorrectly read '3' (byte 0
    // of this shuffle mask), resulting in reading a 0 value from Shuffle A.
    //
    //          is read instead ---\              /--- should be read
    //                              v            v
    kSimdPrefix, kExprI8x16Shuffle, 3, 0, 0, 0, 24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    // Shuffle C: Demand byte 4 from Shuffle B. This results in a shift of 4
    // demanding byte 0 of the shifted input.
    kSimdPrefix, kExprI8x16Shuffle, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    // Shuffle D: Demand byte 0 from Shuffle C as the first 2 bytes only require
    // byte 0.
    kSimdPrefix, kExprI8x16Shuffle, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // Extract Lane: Demand byte 0-1 from Shuffle D.
    kSimdPrefix, kExprI16x8ExtractLaneU, 0,
  ]);

const instance = builder.instantiate();

let liftoff = instance.exports.main();
%WasmTierUpFunction(instance.exports.main);
let turbofan = instance.exports.main();
assertEquals(liftoff, turbofan);
