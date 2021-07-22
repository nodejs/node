// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32, false);
builder.addType(makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]));
builder.addFunction(undefined, 0 /* sig */)
  .addBodyWithEnd([
// signature: i_iii
// body:
kExprLocalGet, 0x01,
kExprLocalGet, 0x01,
kExprLocalGet, 0x01,
kExprLocalGet, 0x01,
kAtomicPrefix, kExprI32AtomicCompareExchange16U, 0x00, 0x7a,
kExprLocalGet, 0x01,
kExprLocalGet, 0x01,
kExprLocalGet, 0x01,
kExprLocalGet, 0x00,
kExprMemoryGrow, 0x00,
kAtomicPrefix, kExprI32AtomicCompareExchange16U, 0x00, 0x7a,
kExprLocalGet, 0x01,
kExprLocalGet, 0x00,
kAtomicPrefix, kExprI32AtomicCompareExchange16U, 0x00, 0x7a,
kExprLocalGet, 0x01,
kExprLocalGet, 0x00,
kAtomicPrefix, kExprI32AtomicCompareExchange16U, 0x00, 0x7a,
kExprLocalGet, 0x01,
kExprReturnCall, 0x00,
kExprEnd,
]);
builder.addExport('main', 0);
const instance = builder.instantiate();
assertTraps(kTrapUnalignedAccess, () => instance.exports.main(0, 0, 0));
