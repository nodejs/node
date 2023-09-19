// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --turbo-force-mid-tier-regalloc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32, false);
// Generate function 1 (out of 3).
builder.addFunction(undefined, makeSig([kWasmI32, kWasmI32, kWasmI32], []))
  .addLocals(kWasmS128, 1)
  .addBody([
    kExprTry, kWasmVoid,  // try i32
      kExprF64Const, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const
      kExprI32Const, 0x00,  // i32.const
      kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
      kExprLocalTee, 0x03,  // local.tee
      kExprCallFunction, 2,  // call function #2
      kExprF64Const, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const
      kExprLocalGet, 0x03,  // local.get
      kExprCallFunction, 2,  // call function #2
      kExprTry, kWasmVoid,  // try
        kExprLocalGet, 0x03,  // local.get
        kExprF64Const, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const
        kExprF32Const, 0x00, 0x00, 0x00, 0x00,  // f32.const
        kExprCallFunction, 1,  // call function #1
      kExprCatchAll,  // catch-all
        kExprEnd,  // end
      kExprI32Const, 0x00,  // i32.const
      kExprI32Const, 0x00,  // i32.const
      kExprI32Const, 0x00,  // i32.const
      kAtomicPrefix, kExprI32AtomicCompareExchange16U, 0x01, 0x80, 0x80, 0xc0, 0x9b, 0x07,  // i32.atomic.cmpxchng16_u
      kExprDrop,  // drop
    kExprCatchAll,  // catch-all
      kExprEnd,  // end
    kExprI32Const, 0x00,  // i32.const
    kSimdPrefix, kExprI8x16Splat,  // i8x16.splat
    kExprF64Const, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const
    kExprF32Const, 0x00, 0x00, 0x00, 0x00,  // f32.const
    kExprCallFunction, 1,  // call function #1
]);
// Generate function 2 (out of 3).
builder.addFunction(undefined, makeSig([kWasmS128, kWasmF64, kWasmF32], []))
    .addBody([kExprUnreachable]);
// Generate function 3 (out of 3).
builder.addFunction(undefined, makeSig([kWasmF64, kWasmS128], [])).addBody([
  kExprUnreachable
]);
builder.toModule();
