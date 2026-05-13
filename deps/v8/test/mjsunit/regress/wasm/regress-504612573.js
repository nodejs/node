// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// Flags: --experimental-wasm-fp16

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1, false);
builder.addFunction('test', makeSig([], [kWasmI32]))
  .addBody([
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kExprI32Const, 16,
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kSimdPrefix, 161, 2, 7, // f16x8.extract_lane(v1, 7)
    kSimdPrefix, 32, 0,       // f32x4.replace_lane(v0, 0)
    kSimdPrefix, kExprI32x4ExtractLane, 0,
  ])
  .exportFunc();

builder.instantiate().exports.test();
