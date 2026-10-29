// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --future-wasm-simd-opt --allow-natives-syntax
d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.addFunction('test', makeSig([], [kWasmI32]))
  .addBody([
    // Vector A = [10, 0, 0, ...]
    kSimdPrefix, kExprS128Const, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // Vector B = [0, ..., 30] (byte 15 = 30)
    kSimdPrefix, kExprS128Const, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 30,
    // s1 = shuffle(A, B, [0, 31, 0, ...]) -> byte 0 = 10, byte 1 = 30
    kSimdPrefix, kExprI8x16Shuffle, 0, 31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // Vector Zero
    kSimdPrefix, kExprS128Const, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // s2 = shuffle(s1, Zero, [1, 0, ...]) -> byte 0 = 30, byte 1 = 10
    kSimdPrefix, kExprI8x16Shuffle, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // Vector Zero
    kSimdPrefix, kExprS128Const, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // s3 = shuffle(s2, Zero, [1, 0, ...]) -> byte 0 = 10, byte 1 = 30
    kSimdPrefix, kExprI8x16Shuffle, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // extract lane 0 -> must be 10
    ...SimdInstr(kExprI8x16ExtractLaneU), 0,
  ])
  .exportFunc();

let instance = builder.instantiate();
assertEquals(10, instance.exports.test());
%WasmTierUpFunction(instance.exports.test);
assertEquals(10, instance.exports.test());
