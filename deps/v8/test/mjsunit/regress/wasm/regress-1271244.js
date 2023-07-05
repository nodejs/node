// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --turbo-force-mid-tier-regalloc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('main', makeSig([], [kWasmI32, kWasmF64, kWasmF64]))
    .addBody([
      kExprI32Const, 1,                        // i32.const
      kSimdPrefix, kExprI8x16Splat,            // i8x16.splat
      kSimdPrefix, kExprF64x2PromoteLowF32x4,  // f64x2.promote_low_f32x4
      kSimdPrefix, kExprI8x16ExtractLaneS, 0,  // i8x16.extract_lane_s
      ...wasmF64Const(2),                      // f64.const
      ...wasmF64Const(1),                      // f64.const
    ]);
builder.toModule();
