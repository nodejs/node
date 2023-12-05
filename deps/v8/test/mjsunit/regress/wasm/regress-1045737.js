// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging --liftoff --no-wasm-tier-up

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
const builder = new WasmModuleBuilder();
builder.addMemory(16, 32, true);
builder.addType(makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]));
// Generate function 1 (out of 1).
builder.addFunction(undefined, 0 /* sig */).addBodyWithEnd([
  // signature: i_iii
  // body:
  kExprI32Const, 0x00, kExprI64Const, 0xc2, 0xe6, 0x00, kAtomicPrefix,
  kExprI64AtomicAdd8U, 0x00, 0xb6, 0x0e, kExprF32SConvertI64,
  kExprI32SConvertF32,
  kExprEnd,  // @14
]);
builder.addExport('main', 0);
const instance = builder.instantiate();
assertEquals(instance.exports.main(1, 2, 3), 0);
})();
