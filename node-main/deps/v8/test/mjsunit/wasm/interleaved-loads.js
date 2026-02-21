// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff
// Flags: --experimental-wasm-simd-opt

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/value-helper.js');

(function ConsecutiveOffsetLoadInterleaveTwoI64x2() {
  print(arguments.callee.name);
  const simd = [
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kExprLocalTee, 0,
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 16,
    kExprLocalTee, 1,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   // lanes: [0, 2]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16Shuffle,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,   // lanes: [1, 3]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    ...SimdInstr(kExprI64x2Sub),
    kExprLocalTee, 2,
    kSimdPrefix, kExprI64x2ExtractLane, 0,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI64x2ExtractLane, 1,
    kExprI64Add,
  ];
  const scalar = [
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 0,    // 0
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 8,    // 1
    kExprI64Sub,
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 16,   // 2
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 24,   // 3
    kExprI64Sub,
    kExprI64Add,
  ];
  const builder = new WasmModuleBuilder();
  builder.addMemory(1);
  builder.exportMemoryAs('memory');
  builder.addFunction("simd", kSig_l_v).addLocals(kWasmS128, 3).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_l_v).addBody(scalar).exportFunc();
  const instance = builder.instantiate();
  const buffer = instance.exports.memory.buffer;
  const src_view = new BigUint64Array(buffer, 0, 4);
  const wasm = instance.exports;
  for (let a of uint64_array) {
    for (let b of uint64_array) {
      const values = [ a, b, b, a ];
      src_view.set(values, 0);
      assertEquals(wasm.scalar(), wasm.simd());
    }
  }
})();

(function ConsecutiveOffsetLoadInterleaveTwoI64x2SwapLoads() {
  print(arguments.callee.name);
  const simd = [
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 16,
    kExprLocalSet, 1,
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kExprLocalTee, 0,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   // lanes: [0, 2]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16Shuffle,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,   // lanes: [1, 3]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    ...SimdInstr(kExprI64x2Sub),
    kExprLocalTee, 2,
    kSimdPrefix, kExprI64x2ExtractLane, 0,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI64x2ExtractLane, 1,
    kExprI64Add,
  ];
  const scalar = [
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 0,    // 0
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 8,    // 1
    kExprI64Sub,
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 16,   // 2
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 24,   // 3
    kExprI64Sub,
    kExprI64Add,
  ];
  const builder = new WasmModuleBuilder();
  builder.addMemory(1);
  builder.exportMemoryAs('memory');
  builder.addFunction("simd", kSig_l_v).addLocals(kWasmS128, 3).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_l_v).addBody(scalar).exportFunc();
  const instance = builder.instantiate();
  const buffer = instance.exports.memory.buffer;
  const src_view = new BigUint64Array(buffer, 0, 4);
  const wasm = instance.exports;
  for (let a of uint64_array) {
    for (let b of uint64_array) {
      const values = [ a, b, b, a ];
      src_view.set(values, 0);
      assertEquals(wasm.scalar(), wasm.simd());
    }
  }
})();

(function ConsecutiveOffsetLoadInterleaveTwoI64x2SwapShuffleOps() {
  print(arguments.callee.name);
  const simd = [
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kExprLocalSet, 0,
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 16,
    kExprLocalTee, 1,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   // lanes: [0, 2]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,   // offsets: #16, #0
    kExprLocalGet, 1,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI8x16Shuffle,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,   // lanes: [1, 3]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   // offsets: #8, #24
    ...SimdInstr(kExprI64x2Sub),
    kExprLocalTee, 2,
    kSimdPrefix, kExprI64x2ExtractLane, 0,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI64x2ExtractLane, 1,
    kExprI64Add,
  ];
  const scalar = [
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 16,
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 8,
    kExprI64Sub,
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 0,
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 24,
    kExprI64Sub,
    kExprI64Add,
  ];
  const builder = new WasmModuleBuilder();
  builder.addMemory(1);
  builder.exportMemoryAs('memory');
  builder.addFunction("simd", kSig_l_v).addLocals(kWasmS128, 3).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_l_v).addBody(scalar).exportFunc();
  const instance = builder.instantiate();
  const buffer = instance.exports.memory.buffer;
  const src_view = new BigUint64Array(buffer, 0, 4);
  const wasm = instance.exports;
  for (let a of uint64_array) {
    for (let b of uint64_array) {
      const values = [ a, b, b, a ];
      src_view.set(values, 0);
      assertEquals(wasm.scalar(), wasm.simd());
    }
  }
})();

(function ConsecutiveOffsetNotZeroLoadInterleaveTwoI64x2() {
  print(arguments.callee.name);
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 16,
    kExprLocalTee, 1,
    kExprLocalGet, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 32,
    kExprLocalTee, 2,
    kSimdPrefix, kExprI8x16Shuffle,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,   // lanes: [1, 3]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   // offsets: #24, #40
    kExprLocalGet, 1,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   // lanes: [0, 2]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,   // offsets: #16, #32
    ...SimdInstr(kExprI64x2Sub),
    kExprLocalTee, 3,
    kSimdPrefix, kExprI64x2ExtractLane, 0,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI64x2ExtractLane, 1,
    kExprI64Sub,
  ];
  const scalar = [
    kExprLocalGet, 0,
    kExprI64LoadMem, 0, 24,    // lane 1
    kExprLocalGet, 0,
    kExprI64LoadMem, 0, 16,    // lane 0
    kExprI64Sub,
    kExprLocalGet, 0,
    kExprI64LoadMem, 0, 40,   // lane 3
    kExprLocalGet, 0,
    kExprI64LoadMem, 0, 32,   // lane 4
    kExprI64Sub,
    kExprI64Sub,
  ];
  const builder = new WasmModuleBuilder();
  builder.addMemory(1);
  builder.exportMemoryAs('memory');
  builder.addFunction("simd", kSig_l_i).addLocals(kWasmS128, 3).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_l_i).addBody(scalar).exportFunc();
  const instance = builder.instantiate();
  const buffer = instance.exports.memory.buffer;
  const src_view = new BigUint64Array(buffer, 0, 8);
  const wasm = instance.exports;
  for (let a of uint64_array) {
    for (let b of uint64_array) {
      const base = 0;
      const values = [ a, b, b, a, a, b, a, b ];
      src_view.set(values, 0);
      assertEquals(wasm.scalar(base), wasm.simd(base));
    }
  }
})();

(function NotConsecutiveOffsetLoadInterleaveTwoI64x2() {
  print(arguments.callee.name);
  const simd = [
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kExprLocalTee, 0,
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 32,
    kExprLocalTee, 1,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   // lanes: [0, 2]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16Shuffle,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,   // lanes: [1, 3]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    ...SimdInstr(kExprI64x2Sub),
    kExprLocalTee, 2,
    kSimdPrefix, kExprI64x2ExtractLane, 0,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI64x2ExtractLane, 1,
    kExprI64Add,
  ];
  const scalar = [
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 0,    // 0
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 8,    // 1
    kExprI64Sub,
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 32,   // 2
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 40,   // 3
    kExprI64Sub,
    kExprI64Add,
  ];
  const builder = new WasmModuleBuilder();
  builder.addMemory(1);
  builder.exportMemoryAs('memory');
  builder.addFunction("simd", kSig_l_v).addLocals(kWasmS128, 3).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_l_v).addBody(scalar).exportFunc();
  const instance = builder.instantiate();
  const buffer = instance.exports.memory.buffer;
  const src_view = new BigUint64Array(buffer, 0, 8);
  const wasm = instance.exports;
  for (let a of uint64_array) {
    for (let b of uint64_array) {
      const values = [ a, b, b, a, a, b, a, b ];
      src_view.set(values, 0);
      assertEquals(wasm.scalar(), wasm.simd());
    }
  }
})();

(function ConsecutiveIndexLoadInterleaveTwoI64x2() {
  print(arguments.callee.name);
  const simd = [
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kExprLocalTee, 0,
    kExprI32Const, 16,
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kExprLocalTee, 1,
    kSimdPrefix, kExprI8x16Shuffle,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,   // lanes: [1, 3]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   // lanes: [0, 2]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    ...SimdInstr(kExprI64x2Sub),
    kExprLocalTee, 2,
    kSimdPrefix, kExprI64x2ExtractLane, 0,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI64x2ExtractLane, 1,
    kExprI64Add,
  ];
  const scalar = [
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 8,    // 1
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 0,    // 0
    kExprI64Sub,
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 24,   // 3
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 16,   // 2
    kExprI64Sub,
    kExprI64Add,
  ];
  const builder = new WasmModuleBuilder();
  builder.addMemory(1);
  builder.exportMemoryAs('memory');
  builder.addFunction("simd", kSig_l_v).addLocals(kWasmS128, 3).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_l_v).addBody(scalar).exportFunc();
  const instance = builder.instantiate();
  const buffer = instance.exports.memory.buffer;
  const src_view = new BigUint64Array(buffer, 0, 4);
  const wasm = instance.exports;
  for (let a of uint64_array) {
    for (let b of uint64_array) {
      const values = [ a, b, b, a ];
      src_view.set(values, 0);
      assertEquals(wasm.scalar(), wasm.simd());
    }
  }
})();

(function NotConsecutiveIndexLoadInterleaveTwoI64x2() {
  print(arguments.callee.name);
  const simd = [
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kExprLocalTee, 0,
    kExprI32Const, 32,
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kExprLocalTee, 1,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   // lanes: [0, 2]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16Shuffle,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,   // lanes: [1, 3]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    ...SimdInstr(kExprI64x2Sub),
    kExprLocalTee, 2,
    kSimdPrefix, kExprI64x2ExtractLane, 0,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI64x2ExtractLane, 1,
    kExprI64Add,
  ];
  const scalar = [
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 0,    // 0
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 8,    // 1
    kExprI64Sub,
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 32,   // 2
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 40,   // 3
    kExprI64Sub,
    kExprI64Add,
  ];
  const builder = new WasmModuleBuilder();
  builder.addMemory(1);
  builder.exportMemoryAs('memory');
  builder.addFunction("simd", kSig_l_v).addLocals(kWasmS128, 3).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_l_v).addBody(scalar).exportFunc();
  const instance = builder.instantiate();
  const buffer = instance.exports.memory.buffer;
  const src_view = new BigUint64Array(buffer, 0, 8);
  const wasm = instance.exports;
  for (let a of uint64_array) {
    for (let b of uint64_array) {
      const values = [ a, b, b, a, b, a, a, b ];
      src_view.set(values, 0);
      assertEquals(wasm.scalar(), wasm.simd());
    }
  }
})();

(function StoreBetweenLoadsI64x2() {
  print(arguments.callee.name);
  const simd = [
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kExprLocalTee, 0,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI8x16Shuffle,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   // lanes: [1, 0]
    kExprLocalSet, 2,
    kExprI32Const, 0,
    kExprLocalGet, 2,
    kSimdPrefix, kExprS128StoreMem, 0, 0,
    kExprI32Const, 16,
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kExprLocalSet, 1,
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   // lanes: [0, 2]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16Shuffle,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,   // lanes: [1, 3]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    ...SimdInstr(kExprI64x2Sub),
    kExprLocalTee, 3,
    kSimdPrefix, kExprI64x2ExtractLane, 0,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI64x2ExtractLane, 1,
    kExprI64Add,
  ];
  const scalar = [
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 8,    // 1
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 8,    // 1
    kExprI64Sub,
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 0,   // 0
    kExprI32Const, 0,
    kExprI64LoadMem, 0, 24,   // 3
    kExprI64Sub,
    kExprI64Add,
  ];
  const builder = new WasmModuleBuilder();
  builder.addMemory(1);
  builder.exportMemoryAs('memory');
  builder.addFunction("simd", kSig_l_v).addLocals(kWasmS128, 4).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_l_v).addBody(scalar).exportFunc();
  const instance = builder.instantiate();
  const buffer = instance.exports.memory.buffer;
  const src_view = new BigUint64Array(buffer, 0, 4);
  const wasm = instance.exports;
  for (let a of uint64_array) {
    for (let b of uint64_array) {
      const values = [ a, b, b, a ];
      src_view.set(values, 0);
      assertEquals(wasm.scalar(), wasm.simd());
    }
  }
})();

(function ConsecutiveOffsetLoadInterleaveTwoI32x4() {
  print(arguments.callee.name);
  const simd = [
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kExprLocalTee, 0,
    kExprI32Const, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 16,
    kExprLocalTee, 1,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0a, 0x0b,   // lanes: [0, 2, 4, 6]
    0x10, 0x11, 0x12, 0x13, 0x18, 0x19, 0x1a, 0x1b,
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16Shuffle,
    0x04, 0x05, 0x06, 0x07, 0x0c, 0x0d, 0x0e, 0x0f,   // lanes: [1, 3, 5, 7]
    0x14, 0x15, 0x16, 0x17, 0x1c, 0x1d, 0x1e, 0x1f,
    ...SimdInstr(kExprI32x4Sub),
    kExprLocalTee, 2,
    kSimdPrefix, kExprI32x4ExtractLane, 0,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI32x4ExtractLane, 1,
    kExprI32Add,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI32x4ExtractLane, 2,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI32x4ExtractLane, 3,
    kExprI32Add,
    kExprI32Add,
  ];
  const scalar = [
    kExprI32Const, 0,
    kExprI32LoadMem, 0, 0,    // 0
    kExprI32Const, 0,
    kExprI32LoadMem, 0, 4,    // 1
    kExprI32Sub,
    kExprI32Const, 0,
    kExprI32LoadMem, 0, 8,   // 2
    kExprI32Const, 0,
    kExprI32LoadMem, 0, 12,   // 3
    kExprI32Sub,
    kExprI32Add,
    kExprI32Const, 0,
    kExprI32LoadMem, 0, 16,   // 4
    kExprI32Const, 0,
    kExprI32LoadMem, 0, 20,   // 5
    kExprI32Sub,
    kExprI32Const, 0,
    kExprI32LoadMem, 0, 24,   // 6
    kExprI32Const, 0,
    kExprI32LoadMem, 0, 28,   // 7
    kExprI32Sub,
    kExprI32Add,
    kExprI32Add,
  ];
  const builder = new WasmModuleBuilder();
  builder.addMemory(1);
  builder.exportMemoryAs('memory');
  builder.addFunction("simd", kSig_i_v).addLocals(kWasmS128, 3).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_i_v).addBody(scalar).exportFunc();
  const instance = builder.instantiate();
  const buffer = instance.exports.memory.buffer;
  const src_view = new Int32Array(buffer, 0, 8);
  const wasm = instance.exports;
  for (let a of int32_array) {
    for (let b of int32_array) {
      const values = [ a, b, b, a, b, a, a, b ];
      src_view.set(values, 0);
      assertEquals(wasm.scalar(), wasm.simd());
    }
  }
})();

(function NegativeConsecutiveLoadInterleaveTwoF32x4() {
  print(arguments.callee.name);
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 32,             // a
    kExprLocalTee, 1,
    kExprLocalGet, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 16,             // b
    kExprLocalTee, 2,
    kSimdPrefix, kExprI8x16Shuffle,
    0x04, 0x05, 0x06, 0x07, 0x0c, 0x0d, 0x0e, 0x0f,   // lanes: [1, 3, 5, 7]
    0x14, 0x15, 0x16, 0x17, 0x1c, 0x1d, 0x1e, 0x1f,   // [a1, a3, b1, b3]
    kExprLocalGet, 1,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0a, 0x0b,   // lanes: [0, 2, 4, 6]
    0x10, 0x11, 0x12, 0x13, 0x18, 0x19, 0x1a, 0x1b,   // [a0, a2, b0, b2]
    ...SimdInstr(kExprF32x4Sub),
    kExprLocalTee, 3,
    kSimdPrefix, kExprF32x4ExtractLane, 0,
    kExprLocalGet, 3,
    kSimdPrefix, kExprF32x4ExtractLane, 1,
    kExprF32Sub,
    kExprLocalGet, 3,
    kSimdPrefix, kExprF32x4ExtractLane, 2,
    kExprLocalGet, 3,
    kSimdPrefix, kExprF32x4ExtractLane, 3,
    kExprF32Add,
    kExprF32Add,
  ];
  const scalar = [
    kExprLocalGet, 0,
    kExprF32LoadMem, 0, 36,   // 5
    kExprLocalGet, 0,
    kExprF32LoadMem, 0, 32,   // 4
    kExprF32Sub,
    kExprLocalGet, 0,
    kExprF32LoadMem, 0, 44,   // 7
    kExprLocalGet, 0,
    kExprF32LoadMem, 0, 40,   // 6
    kExprF32Sub,
    kExprF32Sub,
    kExprLocalGet, 0,
    kExprF32LoadMem, 0, 20,   // 1
    kExprLocalGet, 0,
    kExprF32LoadMem, 0, 16,   // 0
    kExprF32Sub,
    kExprLocalGet, 0,
    kExprF32LoadMem, 0, 28,   // 3
    kExprLocalGet, 0,
    kExprF32LoadMem, 0, 24,   // 2
    kExprF32Sub,
    kExprF32Add,
    kExprF32Add,
  ];
  const builder = new WasmModuleBuilder();
  builder.addMemory(1);
  builder.exportMemoryAs('memory');
  builder.addFunction("simd", kSig_f_i).addLocals(kWasmS128, 3).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_f_i).addBody(scalar).exportFunc();
  const instance = builder.instantiate();
  const buffer = instance.exports.memory.buffer;
  const src_view = new Float32Array(buffer, 0, 12);
  const wasm = instance.exports;
  for (let a of float32_array) {
    for (let b of float32_array) {
      const base = 0;
      const values = [ a, b, a, a, b, b, a, a, b, a, a, b ];
      src_view.set(values, 0);
      assertEquals(wasm.scalar(), wasm.simd(base));
    }
  }
})();

(function ConsecutiveNotZeroLoadInterleaveTwoI16x8() {
  print(arguments.callee.name);
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 4,             // a
    kExprLocalTee, 1,
    kExprLocalGet, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 20,             // b
    kExprLocalTee, 2,
    kSimdPrefix, kExprI8x16Shuffle,
    0x02, 0x03, 0x06, 0x07, 0x0a, 0x0b, 0x0e, 0x0f,   // lanes: [1, 3, 5, 7, 9, 11, 13, 15]
    0x12, 0x13, 0x16, 0x17, 0x1a, 0x1b, 0x1e, 0x1f,   // [a1, a3, a5, a7, b1, b3, b5, b7]
    kExprLocalGet, 1,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x04, 0x05, 0x08, 0x09, 0x0c, 0x0d,   // lanes: [0, 2, 4, 6, 8, 10, 12, 14]
    0x10, 0x11, 0x14, 0x15, 0x18, 0x19, 0x1c, 0x1d,   // [a0, a2, a4, a6, b0, b2, b4, b6]
    ...SimdInstr(kExprI32x4DotI16x8S),                // a0*a1+b0*b1, a2*a3+b2*b3, a4*a5+b4*b5, a6*a7+b6*b7
    kExprLocalTee, 3,
    kSimdPrefix, kExprI32x4ExtractLane, 0,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI32x4ExtractLane, 1,
    kExprI32Add,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI32x4ExtractLane, 2,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI32x4ExtractLane, 3,
    kExprI32Add,
    kExprI32Add,
  ];
  const scalar = [
    kExprLocalGet, 0,
    kExprI32LoadMem16S, 0, 6,     // 1
    kExprLocalGet, 0,
    kExprI32LoadMem16S, 0, 4,     // 0
    kExprI32Mul,                  // a0*a1
    kExprLocalGet, 0,
    kExprI32LoadMem16S, 0, 22,    // 9
    kExprLocalGet, 0,
    kExprI32LoadMem16S, 0, 20,    // 8
    kExprI32Mul,                  // b0*b1
    kExprI32Add,
    kExprLocalGet, 0,
    kExprI32LoadMem16S, 0, 10,    // 3
    kExprLocalGet, 0,
    kExprI32LoadMem16S, 0, 8,     // 2
    kExprI32Mul,                  // a2*a3
    kExprLocalGet, 0,
    kExprI32LoadMem16S, 0, 26,    // 11
    kExprLocalGet, 0,
    kExprI32LoadMem16S, 0, 24,    // 10
    kExprI32Mul,                  // b2*b3
    kExprI32Add,
    kExprLocalGet, 0,
    kExprI32LoadMem16S, 0, 14,    // 5
    kExprLocalGet, 0,
    kExprI32LoadMem16S, 0, 12,    // 4
    kExprI32Mul,                  // a4*a5
    kExprLocalGet, 0,
    kExprI32LoadMem16S, 0, 30,    // 13
    kExprLocalGet, 0,
    kExprI32LoadMem16S, 0, 28,    // 12
    kExprI32Mul,                  // b4*b5
    kExprI32Add,
    kExprLocalGet, 0,
    kExprI32LoadMem16S, 0, 18,    // 7
    kExprLocalGet, 0,
    kExprI32LoadMem16S, 0, 16,    // 6
    kExprI32Mul,                  // a6*a7
    kExprLocalGet, 0,
    kExprI32LoadMem16S, 0, 34,    // 15
    kExprLocalGet, 0,
    kExprI32LoadMem16S, 0, 32,    // 14
    kExprI32Mul,                  // b6*b7
    kExprI32Add,
    kExprI32Add,
    kExprI32Add,
    kExprI32Add,
  ];
  const builder = new WasmModuleBuilder();
  builder.addMemory(1);
  builder.exportMemoryAs('memory');
  builder.addFunction("simd", kSig_i_i).addLocals(kWasmS128, 3).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_i_i).addLocals(kWasmI32, 1).addBody(scalar).exportFunc();
  const instance = builder.instantiate();
  const buffer = instance.exports.memory.buffer;
  const src_view = new Int16Array(buffer, 0, 18);
  const wasm = instance.exports;
  for (let a of int16_array) {
    for (let b of int16_array) {
      const base = 0;
      const values = [ b, b, a, b, a, a, b, b, a, a, b, a, a, b, a, b, a, a ];
      src_view.set(values, 0);
      assertEquals(wasm.scalar(), wasm.simd(base));
    }
  }
})();

(function ConsecutiveNotZeroLoadInterleaveTwoI8x16() {
  print(arguments.callee.name);
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 2,             // a
    kExprLocalTee, 1,
    kExprLocalGet, 0,
    kSimdPrefix, kExprS128LoadMem, 0, 18,             // b
    kExprLocalTee, 2,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e,   // even lanes
    0x10, 0x12, 0x14, 0x16, 0x18, 0x1a, 0x1c, 0x1e,
    kExprLocalGet, 1,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16Shuffle,
    0x01, 0x03, 0x05, 0x07, 0x09, 0x0b, 0x0d, 0x0f,   // odd lanes
    0x11, 0x13, 0x15, 0x17, 0x19, 0x1b, 0x1d, 0x1f,
    ...SimdInstr(kExprI8x16Sub),
    kExprLocalTee, 3,
    kExprLocalGet, 3,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI8x16Shuffle,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,   // top half
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ...SimdInstr(kExprI8x16Add),
    kExprLocalTee, 3,
    kExprLocalGet, 3,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI8x16Shuffle,
    0x04, 0x05, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00,   // top half
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ...SimdInstr(kExprI8x16Add),
    kExprLocalTee, 3,
    kExprLocalGet, 3,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI8x16Shuffle,
    0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // top half
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ...SimdInstr(kExprI8x16Add),
    kExprLocalTee, 3,
    kExprLocalGet, 3,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI8x16Shuffle,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // top half
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ...SimdInstr(kExprI8x16Add),
    kSimdPrefix, kExprI8x16ExtractLaneS, 0,
  ];
  const scalar = [
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 2,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 3,
    kExprI32Sub,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 4,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 5,
    kExprI32Sub,
    kExprI32Add,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 6,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 7,
    kExprI32Sub,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 8,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 9,
    kExprI32Sub,
    kExprI32Add,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 10,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 11,
    kExprI32Sub,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 12,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 13,
    kExprI32Sub,
    kExprI32Add,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 14,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 15,
    kExprI32Sub,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 16,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 17,
    kExprI32Sub,
    kExprI32Add,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 18,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 19,
    kExprI32Sub,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 20,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 21,
    kExprI32Sub,
    kExprI32Add,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 22,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 23,
    kExprI32Sub,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 24,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 25,
    kExprI32Sub,
    kExprI32Add,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 26,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 27,
    kExprI32Sub,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 28,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 29,
    kExprI32Sub,
    kExprI32Add,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 30,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 31,
    kExprI32Sub,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 32,
    kExprLocalGet, 0,
    kExprI32LoadMem8S, 0, 33,
    kExprI32Sub,
    kExprI32Add,
    kExprI32Add,
    kExprI32Add,
    kExprI32Add,
    kExprI32Add,
    kExprI32Add,
    kExprI32Add,
    kExprI32Add,
    ...wasmI32Const(0xFF),
    kExprI32And,
    kExprI32SExtendI8,
  ];
  const builder = new WasmModuleBuilder();
  builder.addMemory(1);
  builder.exportMemoryAs('memory');
  builder.addFunction("simd", kSig_i_i).addLocals(kWasmS128, 3).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_i_i).addBody(scalar).exportFunc();
  const instance = builder.instantiate();
  const buffer = instance.exports.memory.buffer;
  const src_view = new Int8Array(buffer, 0, 34);
  const wasm = instance.exports;
  for (let a of int8_array) {
    for (let b of int8_array) {
      const base = 0;
      const values = [
        a, b, b, a, a, b, a,
        b, a, b, a, b, b, b,
        a, a, b, b, a, a, b,
        b, b, a, a, b, a, a,
        b, a ];
      src_view.set(values, 0);
      assertEquals(wasm.scalar(base), wasm.simd(base));
    }
  }
})();
