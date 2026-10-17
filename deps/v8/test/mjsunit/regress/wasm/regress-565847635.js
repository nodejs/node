// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-fp16 --no-enable-avx2

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.addMemory(1, 1);
builder.addFunction('test_store', kSig_v_f)
  .addBody([
    wasmI32Const(0),
    kExprLocalGet, 0,
    kNumericPrefix, kExprF32StoreF16, 0, 0,
  ].flat())
  .exportFunc();
builder.addFunction('test_load', kSig_f_v)
  .addBody([
    wasmI32Const(0),
    kNumericPrefix, kExprF32LoadF16, 0, 0,
  ].flat())
  .exportFunc();

let instance = builder.instantiate();
instance.exports.test_store(1.5);
assertEquals(1.5, instance.exports.test_load());
