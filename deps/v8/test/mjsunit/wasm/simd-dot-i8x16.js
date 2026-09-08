// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/value-helper.js');

(function I32x4DotI8x16S() {
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.exportMemoryAs('memory');

  const load_offsets = [0, 3, 8, 16, 32];
  const store_offset = 48;
  const inverted_store_offset = store_offset + 16;
  const scalar_store_offset = inverted_store_offset + 16;

  builder.addFunction('simd_dot', kSig_v_iii)
      .addLocals(kWasmS128, 3)
      .addBody([
        kExprLocalGet, 2,
        kExprLocalGet, 0,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprLocalTee, 3,
        kExprLocalGet, 1,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprLocalTee, 4,
        ...SimdInstr(kExprI16x8ExtMulLowI8x16S),
        ...SimdInstr(kExprI32x4ExtAddPairwiseI16x8S),
        kExprLocalGet, 3,
        kExprLocalGet, 4,
        ...SimdInstr(kExprI16x8ExtMulHighI8x16S),
        ...SimdInstr(kExprI32x4ExtAddPairwiseI16x8S),
        ...SimdInstr(kExprI32x4Add),
        kSimdPrefix, kExprS128StoreMem, 0, 0,
      ])
      .exportFunc();

  builder.addFunction('simd_dot_inverted', kSig_v_iii)
      .addLocals(kWasmS128, 3)
      .addBody([
        kExprLocalGet, 2,
        kExprLocalGet, 0,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprLocalTee, 3,
        kExprLocalGet, 1,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprLocalTee, 4,
        ...SimdInstr(kExprI16x8ExtMulHighI8x16S),
        ...SimdInstr(kExprI32x4ExtAddPairwiseI16x8S),
        kExprLocalGet, 3,
        kExprLocalGet, 4,
        ...SimdInstr(kExprI16x8ExtMulLowI8x16S),
        ...SimdInstr(kExprI32x4ExtAddPairwiseI16x8S),
        ...SimdInstr(kExprI32x4Add),
        kSimdPrefix, kExprS128StoreMem, 0, 0,
      ])
      .exportFunc();

  builder.addFunction('scalar_dot', kSig_v_iii)
      .addLocals(kWasmI32, 4)
      .addBody([
        kExprLocalGet, 2,
        kExprLocalGet, 2,
        kExprLocalGet, 2,
        kExprLocalGet, 2,
        // Lane 0: (0,1) + (8,9)
        kExprLocalGet, 0,
        kExprI32LoadMem8S, 0, 0,
        kExprLocalGet, 1,
        kExprI32LoadMem8S, 0, 0,
        kExprI32Mul,
        kExprLocalGet, 0,
        kExprI32LoadMem8S, 0, 1,
        kExprLocalGet, 1,
        kExprI32LoadMem8S, 0, 1,
        kExprI32Mul,
        kExprI32Add,
        kExprLocalGet, 0,
        kExprI32LoadMem8S, 0, 8,
        kExprLocalGet, 1,
        kExprI32LoadMem8S, 0, 8,
        kExprI32Mul,
        kExprI32Add,
        kExprLocalGet, 0,
        kExprI32LoadMem8S, 0, 9,
        kExprLocalGet, 1,
        kExprI32LoadMem8S, 0, 9,
        kExprI32Mul,
        kExprI32Add,
        kExprLocalSet, 2,

        // Lane 1: (2,3) + (10,11)
        kExprLocalGet, 0,
        kExprI32LoadMem8S, 0, 2,
        kExprLocalGet, 1,
        kExprI32LoadMem8S, 0, 2,
        kExprI32Mul,
        kExprLocalGet, 0,
        kExprI32LoadMem8S, 0, 3,
        kExprLocalGet, 1,
        kExprI32LoadMem8S, 0, 3,
        kExprI32Mul,
        kExprI32Add,
        kExprLocalGet, 0,
        kExprI32LoadMem8S, 0, 10,
        kExprLocalGet, 1,
        kExprI32LoadMem8S, 0, 10,
        kExprI32Mul,
        kExprI32Add,
        kExprLocalGet, 0,
        kExprI32LoadMem8S, 0, 11,
        kExprLocalGet, 1,
        kExprI32LoadMem8S, 0, 11,
        kExprI32Mul,
        kExprI32Add,
        kExprLocalSet, 3,

        // Lane 2: (4,5) + (12,13)
        kExprLocalGet, 0,
        kExprI32LoadMem8S, 0, 4,
        kExprLocalGet, 1,
        kExprI32LoadMem8S, 0, 4,
        kExprI32Mul,
        kExprLocalGet, 0,
        kExprI32LoadMem8S, 0, 5,
        kExprLocalGet, 1,
        kExprI32LoadMem8S, 0, 5,
        kExprI32Mul,
        kExprI32Add,
        kExprLocalGet, 0,
        kExprI32LoadMem8S, 0, 12,
        kExprLocalGet, 1,
        kExprI32LoadMem8S, 0, 12,
        kExprI32Mul,
        kExprI32Add,
        kExprLocalGet, 0,
        kExprI32LoadMem8S, 0, 13,
        kExprLocalGet, 1,
        kExprI32LoadMem8S, 0, 13,
        kExprI32Mul,
        kExprI32Add,
        kExprLocalSet, 4,

        // Lane 3: (6,7) + (14,15)
        kExprLocalGet, 0,
        kExprI32LoadMem8S, 0, 6,
        kExprLocalGet, 1,
        kExprI32LoadMem8S, 0, 6,
        kExprI32Mul,
        kExprLocalGet, 0,
        kExprI32LoadMem8S, 0, 7,
        kExprLocalGet, 1,
        kExprI32LoadMem8S, 0, 7,
        kExprI32Mul,
        kExprI32Add,
        kExprLocalGet, 0,
        kExprI32LoadMem8S, 0, 14,
        kExprLocalGet, 1,
        kExprI32LoadMem8S, 0, 14,
        kExprI32Mul,
        kExprI32Add,
        kExprLocalGet, 0,
        kExprI32LoadMem8S, 0, 15,
        kExprLocalGet, 1,
        kExprI32LoadMem8S, 0, 15,
        kExprI32Mul,
        kExprI32Add,
        kExprLocalSet, 5,

        // Store the four lanes.
        kExprLocalGet, 2,
        kExprI32StoreMem, 0, 0,
        kExprLocalGet, 3,
        kExprI32StoreMem, 0, 4,
        kExprLocalGet, 4,
        kExprI32StoreMem, 0, 8,
        kExprLocalGet, 5,
        kExprI32StoreMem, 0, 12,
      ])
      .exportFunc();


  const instance = builder.instantiate();
  const mem = new Int8Array(instance.exports.memory.buffer);
  const simd_result =
      new Int32Array(instance.exports.memory.buffer, store_offset, 4);
  const inverted_simd_result =
      new Int32Array(instance.exports.memory.buffer, inverted_store_offset, 4);
  const scalar_result = new Int32Array(
      instance.exports.memory.buffer, scalar_store_offset, 4);

  for (let i = 0; i < 64; ++i) {
    mem[i] = int8_array[i % int8_array.length];
  }

  for (const left of load_offsets) {
    for (const right of load_offsets) {
      instance.exports.scalar_dot(left, right, scalar_store_offset);
      instance.exports.simd_dot(left, right, store_offset);
      instance.exports.simd_dot_inverted(left, right, inverted_store_offset);
      for (let i = 0; i < 4; ++i) {
        assertEquals(scalar_result[i], simd_result[i]);
        assertEquals(inverted_simd_result[i], simd_result[i]);
      }
    }
  }
})();

(function I32x4DotI8x16SAddReduce() {
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.exportMemoryAs('memory');

  const load_offsets = [0, 3, 8, 16, 32];
  builder.addFunction('simd_dot_reduce', kSig_i_ii)
      .addLocals(kWasmS128, 3)
      .addBody([
        kExprLocalGet, 0,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprLocalTee, 2,
        kExprLocalGet, 1,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprLocalTee, 3,
        ...SimdInstr(kExprI16x8ExtMulLowI8x16S),
        ...SimdInstr(kExprI32x4ExtAddPairwiseI16x8S),
        kExprLocalGet, 2,
        kExprLocalGet, 3,
        ...SimdInstr(kExprI16x8ExtMulHighI8x16S),
        ...SimdInstr(kExprI32x4ExtAddPairwiseI16x8S),
        ...SimdInstr(kExprI32x4Add),
        kExprLocalTee, 4,
        kSimdPrefix, kExprI32x4ExtractLane, 0,
        kExprLocalGet, 4,
        kSimdPrefix, kExprI32x4ExtractLane, 1,
        kExprLocalGet, 4,
        kSimdPrefix, kExprI32x4ExtractLane, 2,
        kExprLocalGet, 4,
        kSimdPrefix, kExprI32x4ExtractLane, 3,
        kExprI32Add,
        kExprI32Add,
        kExprI32Add,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const mem = new Int8Array(instance.exports.memory.buffer);
  for (let i = 0; i < 64; ++i) {
    mem[i] = int8_array[i % int8_array.length];
  }

  function expectedDotReduce(left, right) {
    let sum = 0;
    for (let i = 0; i < 16; ++i) {
      sum += mem[left + i] * mem[right + i];
    }
    return sum;
  }

  for (const left of load_offsets) {
    for (const right of load_offsets) {
      assertEquals(
          expectedDotReduce(left, right),
          instance.exports.simd_dot_reduce(left, right));
    }
  }
})();

(function I32x4DotI8x16SAddReduceLoopWithTwoDotChains() {
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.exportMemoryAs('memory');

  const load_offsets = [0, 3, 8, 16, 32];
  const sig = makeSig(
      [kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32], [kWasmI32]);

  builder.addFunction('simd_two_dot_chains_loop_reduce', sig)
      .addLocals(kWasmS128, 3)
      .addBody([
        ...wasmS128Const(new Array(16).fill(0)),
        kExprLocalSet, 5,

        kExprBlock, kWasmVoid,
          kExprLoop, kWasmVoid,
            kExprLocalGet, 4,
            kExprI32Eqz,
            kExprBrIf, 1,

            kExprLocalGet, 5,

            kExprLocalGet, 0,
            kSimdPrefix, kExprS128LoadMem, 0, 0,
            kExprLocalTee, 6,
            kExprLocalGet, 1,
            kSimdPrefix, kExprS128LoadMem, 0, 0,
            kExprLocalTee, 7,
            ...SimdInstr(kExprI16x8ExtMulLowI8x16S),
            ...SimdInstr(kExprI32x4ExtAddPairwiseI16x8S),
            kExprLocalGet, 6,
            kExprLocalGet, 7,
            ...SimdInstr(kExprI16x8ExtMulHighI8x16S),
            ...SimdInstr(kExprI32x4ExtAddPairwiseI16x8S),
            ...SimdInstr(kExprI32x4Add),

            kExprLocalGet, 2,
            kSimdPrefix, kExprS128LoadMem, 0, 0,
            kExprLocalTee, 6,
            kExprLocalGet, 3,
            kSimdPrefix, kExprS128LoadMem, 0, 0,
            kExprLocalTee, 7,
            ...SimdInstr(kExprI16x8ExtMulLowI8x16S),
            ...SimdInstr(kExprI32x4ExtAddPairwiseI16x8S),
            kExprLocalGet, 6,
            kExprLocalGet, 7,
            ...SimdInstr(kExprI16x8ExtMulHighI8x16S),
            ...SimdInstr(kExprI32x4ExtAddPairwiseI16x8S),
            ...SimdInstr(kExprI32x4Add),

            ...SimdInstr(kExprI32x4Add),
            ...SimdInstr(kExprI32x4Add),
            kExprLocalSet, 5,

            kExprLocalGet, 4,
            kExprI32Const, 1,
            kExprI32Sub,
            kExprLocalSet, 4,
            kExprBr, 0,
          kExprEnd,
        kExprEnd,

        kExprLocalGet, 5,
        kExprLocalTee, 6,
        kSimdPrefix, kExprI32x4ExtractLane, 0,
        kExprLocalGet, 6,
        kSimdPrefix, kExprI32x4ExtractLane, 1,
        kExprLocalGet, 6,
        kSimdPrefix, kExprI32x4ExtractLane, 2,
        kExprLocalGet, 6,
        kSimdPrefix, kExprI32x4ExtractLane, 3,
        kExprI32Add,
        kExprI32Add,
        kExprI32Add,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const mem = new Int8Array(instance.exports.memory.buffer);
  for (let i = 0; i < 128; ++i) {
    mem[i] = int8_array[i % int8_array.length];
  }

  function expectedDotReduce(left, right) {
    let sum = 0;
    for (let i = 0; i < 16; ++i) {
      sum += mem[left + i] * mem[right + i];
    }
    return sum;
  }

  for (const left0 of load_offsets) {
    for (const right0 of load_offsets) {
      for (const left1 of load_offsets) {
        for (const right1 of load_offsets) {
          for (const iterations of [0, 1, 2, 5]) {
            assertEquals(
                iterations * (expectedDotReduce(left0, right0) +
                              expectedDotReduce(left1, right1)) | 0,
                instance.exports.simd_two_dot_chains_loop_reduce(
                    left0, right0, left1, right1, iterations));
          }
        }
      }
    }
  }
})();
