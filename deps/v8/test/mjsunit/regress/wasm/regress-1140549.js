// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32, true);
builder.addType(makeSig([], []));
builder.addFunction(undefined, 0 /* sig */)
  .addBodyWithEnd([
// signature: v_v
// body:
kExprI32Const, 0x00,
kExprI32Const, 0x00,
kExprI32Const, 0x00,
kAtomicPrefix, kExprI32AtomicCompareExchange8U, 0x00, 0xc3, 0x01,
kExprDrop,
kExprEnd,  // end @193
]);
builder.addExport('main', 0);
const instance = builder.instantiate();
print(instance.exports.main(1, 2, 3));
