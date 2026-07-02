// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/mjsunit.js');

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1, false);
builder.exportMemoryAs('memory');

builder.addFunction('halve_16', kSig_i_v)
  .addBody([
    ...wasmI32Const(0),
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    ...wasmI32Const(16),
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kSimdPrefix, kExprI8x16Shuffle,
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 10, 11, 12, 13, 14, 17,
    ...SimdInstr(kExprI32x4UConvertI16x8Low),
    ...wasmI32Const(0),
    kSimdPrefix, kExprI32x4Splat,
    kSimdPrefix, kExprI8x16Shuffle,
    1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    kSimdPrefix, kExprI8x16ExtractLaneU, 0,
  ])
  .exportFunc();

builder.addFunction('halve_32', kSig_i_v)
  .addBody([
    ...wasmI32Const(0),
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    ...wasmI32Const(16),
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kSimdPrefix, kExprI8x16Shuffle,
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 10, 11, 12, 13, 14, 15,
    ...SimdInstr(kExprI64x2SConvertI32x4Low),
    ...wasmI32Const(0),
    kSimdPrefix, kExprI32x4Splat,
    kSimdPrefix, kExprI8x16Shuffle,
    3, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    kSimdPrefix, kExprI8x16ExtractLaneU, 0,
  ])
  .exportFunc();

const instance = builder.instantiate();
const memory = new Uint8Array(instance.exports.memory.buffer);
memory[0] = 0x11;
memory[1] = 0x22;
memory[2] = 0x33;
memory[3] = 0x44;

assertEquals(0x22, instance.exports.halve_16());
assertEquals(0x44, instance.exports.halve_32());
