// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  const builder = new WasmModuleBuilder();
  builder.addType(makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]));
  builder.addType(makeSig([], []));
  // Generate function 1 (out of 2).
  builder.addFunction(undefined, 0 /* sig */)
    .addBodyWithEnd([
// signature: i_iii
// body:
kExprCallFunction, 0x01, // function #1: v_v
kExprI32Const, 0x00,
kExprEnd,   // @5
            ]);
  // Generate function 2 (out of 2).
  builder.addFunction(undefined, 1 /* sig */)
    .addLocals(kWasmF32, 1).addLocals(kWasmI32, 13)
    .addBodyWithEnd([
// signature: v_v
// body:
kExprEnd,   // @5
            ]);
  builder.addExport('main', 0);
  const instance = builder.instantiate();
  print(instance.exports.main(1, 2, 3));
})();
