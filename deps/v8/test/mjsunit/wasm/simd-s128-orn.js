// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/value-helper.js');

const builder = new WasmModuleBuilder();

builder.addMemory(16, 32);
builder.addFunction("main_vector", makeSig([kWasmI32, kWasmI32], [kWasmI32]))
.addBody([
  kExprLocalGet, 0,
  kSimdPrefix, kExprI8x16Splat,
  kExprLocalGet, 1,
  kSimdPrefix, kExprI8x16Splat,
  kSimdPrefix, kExprS128Not,
  kSimdPrefix, kExprS128Or,
  kSimdPrefix, kExprI8x16ExtractLaneU, 0,
]).exportFunc();

builder.addFunction("main_scalar", makeSig([kWasmI32, kWasmI32], [kWasmI32]))
.addBody([
  kExprLocalGet, 0,
  kExprLocalGet, 1,
  ...wasmI32Const(0xff),
  kExprI32Xor, // xor with 1111111... is effectively a not operation
  kExprI32Ior,
  ...wasmI32Const(0xff),
  kExprI32And, // and with ff means it'll ignore all the data we don't care about ()
]).exportFunc();

const module = builder.instantiate();

assertEquals(0x99, module.exports.main_vector(0x88,0xee));
assertEquals(0x99, module.exports.main_scalar(0x88,0xee));

for (let a of int8_array) {
  for (let b of int8_array) {
    assertEquals(module.exports.main_vector(a,b), module.exports.main_scalar(a,b));
  }
}
