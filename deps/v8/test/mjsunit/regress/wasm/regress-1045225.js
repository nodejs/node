// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging

load('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  const builder = new WasmModuleBuilder();
  builder.addMemory(16, 32, false, true);
  builder.addType(makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]));
  // Generate function 1 (out of 1).
  builder.addFunction(undefined, 0 /* sig */)
    .addBodyWithEnd([
// signature: i_iii
// body:
kExprI32Const, 0x80, 0x01,
kExprI32Clz,
kExprI32Const, 0x00,
kExprI64Const, 0x00,
kAtomicPrefix, kExprI64AtomicStore8U, 0x00, 0x00,
kExprEnd,   // @13
            ]);
  builder.addExport('main', 0);
  const instance = builder.instantiate();
  print(instance.exports.main(1, 2, 3));
})();
