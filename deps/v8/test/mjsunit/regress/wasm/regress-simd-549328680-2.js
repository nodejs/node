// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --future-wasm-simd-opt --allow-natives-syntax
d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.addFunction('test', makeSig([], [kWasmI32]))
  .addBody([
    // Vector A = [0, 0, ...]
    kSimdPrefix, kExprS128Const, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // Vector B = [0, ..., 42] (byte 15 is 42)
    kSimdPrefix, kExprS128Const, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 42,
    // S2 = shuffle(A, B, [0, 0, ..., 31]) -> S2[0] = 0, S2[15] = 42
    kSimdPrefix, kExprI8x16Shuffle, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 31,
    // Vector Zero
    kSimdPrefix, kExprS128Const, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // S3 = shuffle(S2, Zero, [0, 15, 0, ...]) -> S3[0] = S2[0] = 0, S3[1] = S2[15] = 42
    kSimdPrefix, kExprI8x16Shuffle, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // Vector Zero
    kSimdPrefix, kExprS128Const, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // S4 = shuffle(S3, Zero, [1, 0, 0, ...]) -> S4[0] = S3[1] = 42
    kSimdPrefix, kExprI8x16Shuffle, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // Extract lane 0 -> Expected: 42
    ...SimdInstr(kExprI8x16ExtractLaneU), 0,
  ])
  .exportFunc();

let instance = builder.instantiate();
assertEquals(42, instance.exports.test());
%WasmTierUpFunction(instance.exports.test);
assertEquals(42, instance.exports.test());
