// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32);

// Test I32x4 AddReduce correctness is not affected by any optimisations

builder.addFunction("main_I32x4_AddReduce", makeSig([kWasmI32], [kWasmI32]))
.addLocals(kWasmS128, 1)
.addBody([
  kExprLocalGet, 0,
  ...SimdInstr(kExprI32x4Splat),
  kExprLocalSet, 1,
  kExprLocalGet, 1,
  ...SimdInstr(kExprI32x4ExtractLane),0,
  kExprLocalGet, 1,
  ...SimdInstr(kExprI32x4ExtractLane),1,
  kExprLocalGet, 1,
  ...SimdInstr(kExprI32x4ExtractLane),2,
  kExprLocalGet, 1,
  ...SimdInstr(kExprI32x4ExtractLane),3,
  kExprI32Add,
  kExprI32Add,
  kExprI32Add,
]).exportFunc();

const module = builder.instantiate();

assertEquals(
  module.exports.main_I32x4_AddReduce(12, 0),
  12*4
);

assertEquals(
  module.exports.main_I32x4_AddReduce(123987, 0),
  123987*4
);
