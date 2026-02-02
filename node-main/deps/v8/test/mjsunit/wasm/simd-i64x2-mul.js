// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-enable-avx

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Carefully hand-crafted test case to exercie a codegen bug in Liftoff.  In
// i64x2.mul, non-AVX case, we will overwrite rhs if dst == rhs. The intention
// is to do dst = lhs * rhs, but if dst == rhs && dst != lhs, we will overwrite
// dst (and hence rhs) with lhs, effectively doing lhs^2.
const builder = new WasmModuleBuilder();
builder.addMemory(16, 32);
builder.addFunction(undefined, kSig_l_v)
.addBody([
  kExprI64Const, 0,
  kSimdPrefix, kExprI64x2Splat,
  kExprI64Const, 1,
  kSimdPrefix, kExprI64x2Splat,
  kExprI64Const, 2,
  kSimdPrefix, kExprI64x2Splat,
  kExprCallFunction, 1,
]);

let sig = makeSig([kWasmS128, kWasmS128, kWasmS128], [kWasmI64]);
builder.addFunction(undefined, sig)
.addLocals(kWasmS128, 10)
.addBody([
  kExprLocalGet, 2,  // This is 2 (lhs).
  kExprI64Const, 4,  // This is 4 (rhs).
  kSimdPrefix, kExprI64x2Splat,
  kSimdPrefix, kExprI64x2Mul, 0x01,  // The bug will write 2 to rhs.
  kSimdPrefix, kExprI64x2ExtractLane, 0,
]);
builder.addExport('main', 0);
const module = builder.instantiate();
// Should be 2 * 4, the buggy codegen will give 2 * 2 instead.
assertEquals(8n, module.exports.main());
