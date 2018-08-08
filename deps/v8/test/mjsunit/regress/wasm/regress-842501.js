// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wasm-trap-handler

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  const builder = new WasmModuleBuilder();
  builder.addMemory(16, 32);
  // Generate function 1 (out of 1).
  sig1 = makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]);
  builder.addFunction(undefined, sig1)
    .addBodyWithEnd([
      // signature: i_iii
      // body:
      kExprI32Const, 0xe1, 0xc8, 0xd5, 0x01,
      kExprI32Const, 0xe2, 0xe4, 0x00,
      kExprI32Sub,
      kExprF32Const, 0x00, 0x00, 0x00, 0x00,
      kExprF32Const, 0xc9, 0xc9, 0xc9, 0x00,
      kExprF32Eq,
      kExprI32LoadMem, 0x01, 0xef, 0xec, 0x95, 0x93, 0x07,
      kExprI32Add,
      kExprIf, kWasmStmt,   // @30
      kExprEnd,             // @32
      kExprI32Const, 0xc9, 0x93, 0xdf, 0xcc, 0x7c,
      kExprEnd,             // @39
    ]);
  builder.addExport('main', 0);
  const instance = builder.instantiate();
  assertTraps(kTrapMemOutOfBounds, _ => instance.exports.main(1, 2, 3));
})();
