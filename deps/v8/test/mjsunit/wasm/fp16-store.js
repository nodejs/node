// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-fp16

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function StoreZero() {
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1);

  builder.addFunction("test", makeSig([], []))
    .addBody([
      kExprI32Const, 0,
      // f32.store_f16 with immediate 0.0.
      kExprF32Const, 0, 0, 0, 0,
      kNumericPrefix, kExprF32StoreF16, 0, 0,
    ].flat()).exportFunc();

  const instance = builder.instantiate();
  instance.exports.test();
})();

(function ReuseInput() {
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1);

  builder.addFunction("test", makeSig([kWasmF32], [kWasmF32]))
    .addBody([
      kExprI32Const, 0,
      kExprLocalGet, 0,
      kNumericPrefix, kExprF32StoreF16, 0, 0,
      // Return the input value.
      kExprLocalGet, 0,
    ].flat()).exportFunc();

  const instance = builder.instantiate();
  const test = instance.exports.test;

  assertEquals(1.5, test(1.5));
  %WasmTierUpFunction(test);
  assertEquals(1.5, test(1.5));
})();
