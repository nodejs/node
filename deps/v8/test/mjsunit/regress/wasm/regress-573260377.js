// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-fp16

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Test 1: Valid overlong LEB128 encodings for f32.load_f16 (0xfc 0x30) and
// f32.store_f16 (0xfc 0x31).
(function TestOverlongFp16LoadStore() {
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1);

  builder.addFunction('store_and_load', kSig_f_f)
      .addBody([
        kExprI32Const, 0,
        kExprLocalGet, 0,
        // 5-byte LEB128 encoding of kExprF32StoreMemF16 (0xfc 0x31):
        kNumericPrefix, 0xb1, 0x80, 0x80, 0x00, 0, 0,
        kExprI32Const, 0,
        // 5-byte LEB128 encoding of kExprF32LoadMemF16 (0xfc 0x30):
        kNumericPrefix, 0xb0, 0x80, 0x80, 0x00, 0, 0,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  assertEquals(1.5, instance.exports.store_and_load(1.5));
})();

// Test 2: Ensure WasmFullDecoder and OpcodeLength stay synchronized on
// overlong LEB128 encodings of f32.load_f16 and reject invalid immediates.
(function TestInvalidImmediateAfterOverlongFp16Load() {
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1);

  builder.addFunction('invalid_offset', kSig_v_v)
      .addBody([
        kExprLoop, kWasmVoid,
        kExprI32Const, 0,
        // 4-byte LEB128 encoding of kExprF32LoadMemF16 (0xfc 0x30):
        kNumericPrefix, 0xb0, 0x80, 0x00,
        // 5-byte LEB128 encoding of align = 0:
        0x80, 0x80, 0x80, 0x80, 0x00,
        // 10 bytes of 0x8c (kExprF32Abs), which form an invalid LEB128 offset:
        ...new Array(10).fill(kExprF32Abs),
        kExprDrop,
        kExprEnd,
      ])
      .exportFunc();

  assertThrows(
      () => builder.instantiate().exports.invalid_offset(),
      WebAssembly.CompileError);
})();
