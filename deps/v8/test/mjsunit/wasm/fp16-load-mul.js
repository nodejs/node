// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-fp16

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1);

builder.addFunction("test_mul", kSig_f_v)
  .addBody([
    // Store 1.0 in FP16 (bits 0x3C00) at address 0 and 2
    wasmI32Const(0),
    wasmI32Const(0x3C00),
    kExprI32StoreMem16, 0, 0, // alignment=0, offset=0

    wasmI32Const(2),
    wasmI32Const(0x3C00),
    kExprI32StoreMem16, 0, 0, // alignment=0, offset=0

    // Load both as f32 using f32.load_f16
    wasmI32Const(0),
    kNumericPrefix, kExprF32LoadF16, 0, 0, // f32.load_f16 alignment=0, offset=0

    wasmI32Const(2),
    kNumericPrefix, kExprF32LoadF16, 0, 0, // f32.load_f16 alignment=0, offset=0

    kExprF32Mul,
  ].flat()).exportFunc();

const instance = builder.instantiate();
const result = instance.exports.test_mul();
assertEquals(1.0, result);
