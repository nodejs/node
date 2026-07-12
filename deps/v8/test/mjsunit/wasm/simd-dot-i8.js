// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/value-helper.js');

(function DotI8x8() {
  const configs = [
    {
      simd_convert: kExprI16x8SConvertI8x16Low,
      load: kExprI32LoadMem8S,
      offset: 0,
    },
    {
      simd_convert: kExprI16x8SConvertI8x16High,
      load: kExprI32LoadMem8S,
      offset: 8,
    },
    {
      simd_convert: kExprI16x8UConvertI8x16Low,
      load: kExprI32LoadMem8U,
      offset: 0,
    },
    {
      simd_convert: kExprI16x8UConvertI8x16High,
      load: kExprI32LoadMem8U,
      offset: 8,
    },
  ];

  for (let left in configs) {
    for (let right in configs) {
      const left_convert = configs[left].simd_convert;
      const right_convert = configs[right].simd_convert;
      const builder = new WasmModuleBuilder();
      builder.addMemory(1, 1, false);
      builder.exportMemoryAs('memory');
      builder.addFunction('simd_dotprod', kSig_i_ii).addLocals(kWasmS128, 1)
        .addBody([
          kExprLocalGet, 0,
          kSimdPrefix, kExprS128LoadMem, 0, 0,
          ...SimdInstr(left_convert),
          kExprLocalGet, 1,
          kSimdPrefix, kExprS128LoadMem, 0, 0,
          ...SimdInstr(right_convert),
          ...SimdInstr(kExprI32x4DotI16x8S),
          kExprLocalTee, 2,
          kSimdPrefix, kExprI32x4ExtractLane, 0,
          kExprLocalGet, 2,
          kSimdPrefix, kExprI32x4ExtractLane, 1,
          kExprLocalGet, 2,
          kSimdPrefix, kExprI32x4ExtractLane, 2,
          kExprLocalGet, 2,
          kSimdPrefix, kExprI32x4ExtractLane, 3,
          kExprI32Add,
          kExprI32Add,
          kExprI32Add,
      ]).exportFunc();

      const left_load = configs[left].load;
      const left_offset = configs[left].offset;
      const right_load = configs[right].load;
      const right_offset = configs[right].offset;
      builder.addFunction('scalar_dotprod', kSig_i_ii)
        .addBody([
          kExprLocalGet, 0,
          left_load, 0, left_offset,
          kExprLocalGet, 1,
          right_load, 0, right_offset,
          kExprI32Mul,
          kExprLocalGet, 0,
          left_load, 0, left_offset + 1,
          kExprLocalGet, 1,
          right_load, 0, right_offset + 1,
          kExprI32Mul,
          kExprI32Add,
          kExprLocalGet, 0,
          left_load, 0, left_offset + 2,
          kExprLocalGet, 1,
          right_load, 0, right_offset + 2,
          kExprI32Mul,
          kExprI32Add,
          kExprLocalGet, 0,
          left_load, 0, left_offset + 3,
          kExprLocalGet, 1,
          right_load, 0, right_offset + 3,
          kExprI32Mul,
          kExprI32Add,
          kExprLocalGet, 0,
          left_load, 0, left_offset + 4,
          kExprLocalGet, 1,
          right_load, 0, right_offset + 4,
          kExprI32Mul,
          kExprI32Add,
          kExprLocalGet, 0,
          left_load, 0, left_offset + 5,
          kExprLocalGet, 1,
          right_load, 0, right_offset + 5,
          kExprI32Mul,
          kExprI32Add,
          kExprLocalGet, 0,
          left_load, 0, left_offset + 6,
          kExprLocalGet, 1,
          right_load, 0, right_offset + 6,
          kExprI32Mul,
          kExprI32Add,
          kExprLocalGet, 0,
          left_load, 0, left_offset + 7,
          kExprLocalGet, 1,
          right_load, 0, right_offset + 7,
          kExprI32Mul,
          kExprI32Add,
        ]).exportFunc();

      const module = builder.instantiate();
      // Initialize memory:
      // 2x16-bytes for inputs.
      const result_array = new Uint8Array(module.exports.memory.buffer);
      for (let i = 0; i < 32; ++i) {
        result_array[i] = int8_array[i % int8_array.length];
      }

      assertEquals(module.exports.scalar_dotprod(0, 16),
                   module.exports.simd_dotprod(0, 16));
    }
  }
}());
