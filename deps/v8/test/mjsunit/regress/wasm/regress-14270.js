// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --no-wasm-lazy-compilation

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Tests that we do not reload the instance cache into the exceptional path
// after throw/rethrow.
(function () {
  const builder = new WasmModuleBuilder();

  builder.addMemory(16, 17, false);

  builder.addTag(kSig_v_i);
  builder.addTag(kSig_v_d);

  builder.addFunction(
    "test",
    makeSig([kWasmI32, kWasmF32, kWasmI64, kWasmI32, kWasmF32, kWasmI64],
            [kWasmF32]))
    .addBody([
      kExprTry, 0x7f,  // try @1 i32
        kExprI32Const, 0x00,  // i32.const
        kExprIf, 0x40,  // if @5
          kExprNop,  // nop
        kExprElse,  // else @8
          kExprLoop, 0x40,  // loop @9
            kExprI32Const, 0x00,  // i32.const
            kExprIf, 0x7f,  // if @13 i32
              kExprF64Const, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb0, 0x43,
              kExprThrow, 0x01,  // throw
            kExprElse,  // else @26
              kExprI32Const, 0xec, 0xc6, 0x7e,  // i32.const
            kExprEnd,  // end @31
            kExprBrIf, 0x00,  // br_if depth=0
          kExprEnd,  // end @34
          kExprUnreachable,  // unreachable
        kExprEnd,  // end @36
        kExprUnreachable,  // unreachable
      kExprCatch, 0x01,  // catch @38
        kExprDrop,  // drop
        kExprI32Const, 0x16,  // i32.const
        kExprF32LoadMem, 0x00, 0x00,  // f32.load
        kExprDrop,  // drop
        kExprI32Const, 0x00,  // i32.const
      kExprCatch, 0x00,  // catch @49
        kExprDrop,  // drop
        kExprI32Const, 0x61,  // i32.const
      kExprCatchAll,  // catch_all @54
        kExprI32Const, 0x00,  // i32.const
      kExprEnd,  // end @57
      kExprDrop,  // drop
      kExprF32Const, 0x00, 0x00, 0x10, 0x43,  // f32.const
    ]);

  builder.instantiate();
})();
