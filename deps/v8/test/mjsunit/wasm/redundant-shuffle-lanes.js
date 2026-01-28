// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-simd-opt

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/value-helper.js');

(function LowLeftHighHighRightLow() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 4,                                 // 4: arg 0
    kExprLocalGet, 1,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 5,                                 // 5: arg 1
    kExprLocalGet, 2,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 6,                                 // 6: arg 2
    kExprLocalGet, 3,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 7,                                 // 7: arg 3
    kExprLocalGet, 4,
    kExprLocalGet, 5,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 0, 1
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // lanes: [0, 0, 6, 7]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   // args: [0, 0, 1, 1]
    kExprLocalGet, 6,
    kExprLocalGet, 7,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 2, 3
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // lanes: [0, 0, 4, 5]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,   // args: [2, 2, 3, 3]
    kSimdPrefix, kExprI8x16Shuffle,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   // high half of right into low.
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // low half of left into high.
    kExprLocalTee, 8,                                 // 3, 3, 0, 0
    kSimdPrefix, kExprI32x4ExtractLane, 0,
    kExprLocalGet, 8,
    kSimdPrefix, kExprI32x4ExtractLane, 2,
    kExprI32Sub,
  ];
  const scalar = [
    kExprLocalGet, 3,
    kExprLocalGet, 0,
    kExprI32Sub,
  ];
  builder.addFunction("simd", kSig_i_iiii).addLocals(kWasmS128, 5).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_i_iiii).addBody(scalar).exportFunc();
  const wasm = builder.instantiate().exports;
  for (let i = 0; i < int32_array.length - 3; ++i) {
    const args = int32_array.slice(i, i + 3);
    assertEquals(wasm.simd(...args),
                 wasm.scalar(...args));
  }
})();

(function HighLeftHighHighRightLow() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 4,                                 // 4: arg 0
    kExprLocalGet, 1,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 5,                                 // 5: arg 1
    kExprLocalGet, 2,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 6,                                 // 6: arg 2
    kExprLocalGet, 3,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 7,                                 // 7: arg 3
    kExprLocalGet, 4,
    kExprLocalGet, 5,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 0, 1
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // lanes: [0, 0, 6, 7]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   // args: [0, 0, 1, 1]
    kExprLocalGet, 6,
    kExprLocalGet, 7,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 2, 3
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // lanes: [0, 0, 4, 5]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,   // args: [2, 2, 3, 3]
    kSimdPrefix, kExprI8x16Shuffle,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   // high half of right into low.
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,   // high half of left into high.
    kExprLocalTee, 8,                                 // 3, 3, 1, 1
    kSimdPrefix, kExprI32x4ExtractLane, 0,
    kExprLocalGet, 8,
    kSimdPrefix, kExprI32x4ExtractLane, 2,
    kExprI32Sub,
  ];
  const scalar = [
    kExprLocalGet, 3,
    kExprLocalGet, 1,
    kExprI32Sub,
  ];
  builder.addFunction("simd", kSig_i_iiii).addLocals(kWasmS128, 5).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_i_iiii).addBody(scalar).exportFunc();
  const wasm = builder.instantiate().exports;
  for (let i = 0; i < int32_array.length - 3; ++i) {
    const args = int32_array.slice(i, i + 3);
    assertEquals(wasm.simd(...args),
                 wasm.scalar(...args));
  }
})();

(function HighLeftHighLowRightLow() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 4,                                 // 4: arg 0
    kExprLocalGet, 1,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 5,                                 // 5: arg 1
    kExprLocalGet, 2,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 6,                                 // 6: arg 2
    kExprLocalGet, 3,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 7,                                 // 7: arg 3
    kExprLocalGet, 4,
    kExprLocalGet, 5,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 0, 1
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // lanes: [0, 0, 6, 7]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   // args: [0, 0, 1, 1]
    kExprLocalGet, 6,
    kExprLocalGet, 7,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 2, 3
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // lanes: [0, 0, 4, 5]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,   // args: [2, 2, 3, 3]
    kSimdPrefix, kExprI8x16Shuffle,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,   // low half of right into low.
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,   // high half of left into high.
    kExprLocalTee, 8,                                 // 2, 2, 1, 1
    kSimdPrefix, kExprI32x4ExtractLane, 1,
    kExprLocalGet, 8,
    kSimdPrefix, kExprI32x4ExtractLane, 3,
    kExprI32Sub,
  ];
  const scalar = [
    kExprLocalGet, 2,
    kExprLocalGet, 1,
    kExprI32Sub,
  ];
  builder.addFunction("simd", kSig_i_iiii).addLocals(kWasmS128, 5).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_i_iiii).addBody(scalar).exportFunc();
  const wasm = builder.instantiate().exports;
  for (let i = 0; i < int32_array.length - 3; ++i) {
    const args = int32_array.slice(i, i + 3);
    assertEquals(wasm.simd(...args),
                 wasm.scalar(...args));
  }
})();

(function LowLeftLowLowRightHigh() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 4,                                 // 4: arg 0
    kExprLocalGet, 1,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 5,                                 // 5: arg 1
    kExprLocalGet, 2,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 6,                                 // 6: arg 2
    kExprLocalGet, 3,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 7,                                 // 7: arg 3
    kExprLocalGet, 4,
    kExprLocalGet, 5,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 0, 1
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // lanes: [0, 0, 6, 7]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   // args: [0, 0, 1, 1]
    kExprLocalGet, 6,
    kExprLocalGet, 7,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 2, 3
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // lanes: [0, 0, 4, 5]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,   // args: [2, 2, 3, 3]
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   // low half of left into low.
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,   // low half of right into high.
    kExprLocalTee, 8,                                 // 0, 0, 2, 2
    kSimdPrefix, kExprI32x4ExtractLane, 0,
    kExprLocalGet, 8,
    kSimdPrefix, kExprI32x4ExtractLane, 3,
    kExprI32Sub,
  ];
  const scalar = [
    kExprLocalGet, 0,
    kExprLocalGet, 2,
    kExprI32Sub,
  ];
  builder.addFunction("simd", kSig_i_iiii).addLocals(kWasmS128, 5).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_i_iiii).addBody(scalar).exportFunc();
  const wasm = builder.instantiate().exports;
  for (let i = 0; i < int32_array.length - 3; ++i) {
    const args = int32_array.slice(i, i + 3);
    assertEquals(wasm.simd(...args),
                 wasm.scalar(...args));
  }
})();

(function HighLeftLowLowRightHigh() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 4,                                 // 4: arg 0
    kExprLocalGet, 1,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 5,                                 // 5: arg 1
    kExprLocalGet, 2,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 6,                                 // 6: arg 2
    kExprLocalGet, 3,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 7,                                 // 7: arg 3
    kExprLocalGet, 4,
    kExprLocalGet, 5,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 0, 1
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // lanes: [0, 0, 6, 7]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   // args: [0, 0, 1, 1]
    kExprLocalGet, 6,
    kExprLocalGet, 7,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 2, 3
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // lanes: [0, 0, 4, 5]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,   // args: [2, 2, 3, 3]
    kSimdPrefix, kExprI8x16Shuffle,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,   // high half of left into low.
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,   // low half of right into high.
    kExprLocalTee, 8,                                 // 1, 1, 2, 2
    kSimdPrefix, kExprI32x4ExtractLane, 0,
    kExprLocalGet, 8,
    kSimdPrefix, kExprI32x4ExtractLane, 3,
    kExprI32Sub,
  ];
  const scalar = [
    kExprLocalGet, 1,
    kExprLocalGet, 2,
    kExprI32Sub,
  ];
  builder.addFunction("simd", kSig_i_iiii).addLocals(kWasmS128, 5).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_i_iiii).addBody(scalar).exportFunc();
  const wasm = builder.instantiate().exports;
  for (let i = 0; i < int32_array.length - 3; ++i) {
    const args = int32_array.slice(i, i + 3);
    assertEquals(wasm.simd(...args),
                 wasm.scalar(...args));
  }
})();

(function LowLeftLowHighRightHigh() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 4,                                 // 4: arg 0
    kExprLocalGet, 1,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 5,                                 // 5: arg 1
    kExprLocalGet, 2,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 6,                                 // 6: arg 2
    kExprLocalGet, 3,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 7,                                 // 7: arg 3
    kExprLocalGet, 4,
    kExprLocalGet, 5,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 0, 1
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // lanes: [0, 0, 6, 7]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   // args: [0, 0, 1, 1]
    kExprLocalGet, 6,
    kExprLocalGet, 7,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 2, 3
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // lanes: [0, 0, 4, 5]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,   // args: [2, 2, 3, 3]
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   // low half of left into low.
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   // high half of right into high.
    kExprLocalTee, 8,                                 // 0, 0, 3, 3
    kSimdPrefix, kExprI32x4ExtractLane, 0,
    kExprLocalGet, 8,
    kSimdPrefix, kExprI32x4ExtractLane, 3,
    kExprI32Sub,
  ];
  const scalar = [
    kExprLocalGet, 0,
    kExprLocalGet, 3,
    kExprI32Sub,
  ];
  builder.addFunction("simd", kSig_i_iiii).addLocals(kWasmS128, 5).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_i_iiii).addBody(scalar).exportFunc();
  const wasm = builder.instantiate().exports;
  for (let i = 0; i < int32_array.length - 3; ++i) {
    const args = int32_array.slice(i, i + 3);
    assertEquals(wasm.simd(...args),
                 wasm.scalar(...args));
  }
})();

(function HighLeftLowHighRightHigh() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 4,                                 // 4: arg 0
    kExprLocalGet, 1,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 5,                                 // 5: arg 1
    kExprLocalGet, 2,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 6,                                 // 6: arg 2
    kExprLocalGet, 3,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 7,                                 // 7: arg 3
    kExprLocalGet, 4,
    kExprLocalGet, 5,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 0, 1
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // lanes: [0, 0, 6, 7]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   // args: [0, 0, 1, 1]
    kExprLocalGet, 6,
    kExprLocalGet, 7,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 2, 3
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // lanes: [0, 0, 4, 5]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,   // args: [2, 2, 3, 3]
    kSimdPrefix, kExprI8x16Shuffle,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,   // high half of left into low.
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   // high half of right into high.
    kExprLocalTee, 8,                                 // 1, 1, 3, 3
    kSimdPrefix, kExprI32x4ExtractLane, 0,
    kExprLocalGet, 8,
    kSimdPrefix, kExprI32x4ExtractLane, 3,
    kExprI32Sub,
  ];
  const scalar = [
    kExprLocalGet, 1,
    kExprLocalGet, 3,
    kExprI32Sub,
  ];
  builder.addFunction("simd", kSig_i_iiii).addLocals(kWasmS128, 5).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_i_iiii).addBody(scalar).exportFunc();
  const wasm = builder.instantiate().exports;
  for (let i = 0; i < int32_array.length - 3; ++i) {
    const args = int32_array.slice(i, i + 3);
    assertEquals(wasm.simd(...args),
                 wasm.scalar(...args));
  }
})();

(function ThreeLayerShuffle() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprF64x2Splat,
    kExprLocalSet, 4,                                 // 4: arg 0
    kExprLocalGet, 1,
    kSimdPrefix, kExprF64x2Splat,
    kExprLocalSet, 5,                                 // 5: arg 1
    kExprLocalGet, 2,
    kSimdPrefix, kExprF64x2Splat,
    kExprLocalSet, 6,                                 // 6: arg 2
    kExprLocalGet, 3,
    kSimdPrefix, kExprF64x2Splat,
    kExprLocalSet, 7,                                 // 7: arg 3
    kExprLocalGet, 4,
    kExprLocalGet, 5,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 0, 1
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   // lanes: [0, 2]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,   // res: [0, 1]
    kExprLocalGet, 6,
    kExprLocalGet, 7,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 2, 3
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,   // lanes: [1, 3]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   // res: [2, 3]
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   // lanes: [0, 2]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,   // res: [0, 2]
    kExprLocalGet, 5,
    kExprLocalGet, 4,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 1, 0
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   // lanes: [0, 2]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,   // res: [1, 0]
    kExprLocalGet, 7,
    kExprLocalGet, 6,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 3, 2
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,   // lanes: [1, 3]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   // res: [3, 1]
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   // lanes: [0, 2]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,   // res: [1, 3]
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   // lanes: [0, 2]
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,   // res: [0, 1]
    kExprLocalTee, 8,
    kSimdPrefix, kExprF64x2ExtractLane, 0,
    kExprLocalGet, 8,
    kSimdPrefix, kExprF64x2ExtractLane, 1,
    kExprF64Sub,
  ];
  const scalar = [
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprF64Sub,
  ];
  builder.addFunction("simd", kSig_d_dddd).addLocals(kWasmS128, 5).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_d_dddd).addBody(scalar).exportFunc();
  const wasm = builder.instantiate().exports;
  for (let i = 0; i < float64_array.length - 3; ++i) {
    const args = float64_array.slice(i, i + 3);
    assertEquals(wasm.simd(...args),
                 wasm.scalar(...args));
  }
})();

(function InterleaveFour() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 4,                                 // 4: arg 0
    kExprLocalGet, 1,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 5,                                 // 5: arg 1
    kExprLocalGet, 2,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 6,                                 // 6: arg 2
    kExprLocalGet, 3,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalSet, 7,                                 // 7: arg 3
    kExprLocalGet, 4,
    kExprLocalGet, 5,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 0, 1
    0x04, 0x05, 0x06, 0x07, 0x1c, 0x1d, 0x1e, 0x1f,   // lanes: [1, 7, 0, 0]
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // res: [0, 1, 0, 0]
    kExprLocalGet, 6,
    kExprLocalGet, 7,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 2, 3
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // lanes: [0, 0, 6, 7]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   // res: [2, 2, 3, 3]
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   // lanes: [0, 1, 6, 7]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   // res: [0, 1, 3, 3]
    kExprLocalGet, 4,
    kExprLocalGet, 5,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 0, 1
    0x00, 0x01, 0x02, 0x03, 0x18, 0x19, 0x1a, 0x1b,   // lanes: [0, 6, 0, 0]
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // res: [0, 1, 0, 0]
    kExprLocalGet, 6,
    kExprLocalGet, 7,
    kSimdPrefix, kExprI8x16Shuffle,                   // args: 2, 3
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,   // lanes: [0, 0, 0, 6]
    0x00, 0x01, 0x02, 0x03, 0x18, 0x19, 0x1a, 0x1b,   // res: [2, 2, 2, 3]
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   // lanes: [0, 1, 6, 7]
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   // res: [0, 1, 2, 3]
    ...SimdInstr(kExprI32x4Sub),                      // [0 - 0, 1 - 1, 3 - 2, 3 - 3]
    kExprLocalTee, 8,
    kSimdPrefix, kExprI32x4ExtractLane, 0,
    kExprLocalGet, 8,
    kSimdPrefix, kExprI32x4ExtractLane, 1,
    kExprI32Add,
    kExprLocalGet, 8,
    kSimdPrefix, kExprI32x4ExtractLane, 2,
    kExprLocalGet, 8,
    kSimdPrefix, kExprI32x4ExtractLane, 3,
    kExprI32Add,
    kExprI32Add,
  ];
  const scalar = [
    kExprLocalGet, 3,
    kExprLocalGet, 2,
    kExprI32Sub,
  ];
  builder.addFunction("simd", kSig_i_iiii).addLocals(kWasmS128, 5).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_i_iiii).addBody(scalar).exportFunc();
  const wasm = builder.instantiate().exports;
  for (let i = 0; i < int32_array.length - 3; ++i) {
    const args = int32_array.slice(i, i + 3);
    assertEquals(wasm.simd(...args),
                 wasm.scalar(...args));
  }
})();

(function ShuffleU8x8ConvertLow() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprI8x16Splat,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 1,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 2,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI8x16ReplaceLane, 3,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 5,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 6,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI8x16ReplaceLane, 7,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 9,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 10,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI8x16ReplaceLane, 11,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 13,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 14,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI8x16ReplaceLane, 15,
    kExprLocalTee, 4,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI8x16Splat,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 1,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 2,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI8x16ReplaceLane, 3,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 5,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 6,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI8x16ReplaceLane, 7,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 9,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 10,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI8x16ReplaceLane, 11,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 13,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 14,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI8x16ReplaceLane, 15,
    kExprLocalTee, 5,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x10, 0x01, 0x11, 0x02, 0x12, 0x03, 0x13,
    0x04, 0x14, 0x05, 0x15, 0x06, 0x16, 0x07, 0x17,
    ...SimdInstr(kExprI16x8UConvertI8x16Low),
    ...SimdInstr(kExprI32x4ExtAddPairwiseI16x8U),
    ...SimdInstr(kExprI64x2UConvertI32x4High),
    kExprLocalTee, 6,
    kSimdPrefix, kExprI64x2ExtractLane, 0,
    kExprLocalGet, 6,
    kSimdPrefix, kExprI64x2ExtractLane, 1,
    kExprI64Add,
  ];
  const byte_mask = wasmI32Const(0xFF);
  const scalar = wasmI32Const(0xFF).concat([
    kExprLocalTee, 4,
    kExprLocalGet, 0,
    kExprI32And,
    kExprLocalGet, 4,
    kExprLocalGet, 1,
    kExprI32And,
    kExprI32Add,
    kExprLocalGet, 4,
    kExprLocalGet, 2,
    kExprI32And,
    kExprLocalGet, 4,
    kExprLocalGet, 3,
    kExprI32And,
    kExprI32Add,
    kExprI32Add,
    kExprI64UConvertI32,
  ]);
  builder.addFunction("simd", kSig_l_iiii).addLocals(kWasmS128, 3).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_l_iiii).addLocals(kWasmI32, 1).addBody(scalar).exportFunc();
  const wasm = builder.instantiate().exports;
  for (let i = 0; i < int8_array.length - 3; ++i) {
    const args = int8_array.slice(i, i + 3);
    assertEquals(wasm.simd(...args),
                 wasm.scalar(...args));
  }
})();

(function DeinterleaveU8x4() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const simd = wasmI32Const(0x0).concat([
    kSimdPrefix, kExprI8x16Splat,
    kExprLocalSet, 5,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI8x16Splat,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI8x16ReplaceLane, 1,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 2,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 3,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 4,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 5,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI8x16ReplaceLane, 6,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 8,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 9,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI8x16ReplaceLane, 10,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 11,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI8x16ReplaceLane, 12,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 13,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 14,
    // After all the replace_lane operations above, we have the following
    // vector of locals:
    //           0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
    // locals: [ 0, 3, 2, 2, 1, 1, 3, 0, 2, 2, 3, 1, 3, 2, 1, 0 ]
    kExprLocalTee, 4,
    kExprLocalGet, 4,
    kSimdPrefix, kExprI8x16Shuffle,  // deinterleave 4 bytes, starting from index 3.
    0x03, 0x07, 0x0b, 0x0f, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ...SimdInstr(kExprI16x8UConvertI8x16Low),
    ...SimdInstr(kExprI32x4UConvertI16x8Low), // i32x4 zext, locals: [ 2, 0, 1, 0 ]
    kExprLocalGet, 5,
    ...SimdInstr(kExprI32x4Add),
    kExprLocalGet, 4,
    kExprLocalGet, 4,
    kSimdPrefix, kExprI8x16Shuffle, // deinterleave 4 bytes, starting from index 2.
    0x02, 0x06, 0x0a, 0x0e, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ...SimdInstr(kExprI16x8UConvertI8x16Low),
    ...SimdInstr(kExprI32x4UConvertI16x8Low), // i32x4 zext, locals: [ 2, 3, 3, 1 ]
    ...SimdInstr(kExprI32x4Sub),
    // [
    //    2 - 2,
    //    0 - 3,
    //    1 - 3,
    //    0 - 1,
    // ]
    kExprLocalGet, 4,
    kExprLocalGet, 4,
    kSimdPrefix, kExprI8x16Shuffle, // deinterleave 4 bytes, starting from index 1.
    0x01, 0x05, 0x09, 0x0d, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ...SimdInstr(kExprI16x8UConvertI8x16Low),
    ...SimdInstr(kExprI32x4UConvertI16x8Low), // i32x4 zext, locals: [ 3, 1, 2, 2 ]
     ...SimdInstr(kExprI32x4Mul),
     // [
     //   3 * (2 - 2),
     //   1 * (0 - 3),
     //   2 * (1 - 3),
     //   2 * (0 - 1),
     // ]
    kExprLocalGet, 4,
    kExprLocalGet, 4,
    kSimdPrefix, kExprI8x16Shuffle, // deinterleave 4 bytes, starting from index 0.
    0x00, 0x04, 0x08, 0x0c, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ...SimdInstr(kExprI16x8UConvertI8x16Low),
    ...SimdInstr(kExprI32x4UConvertI16x8Low), // i32x4 zext, locals: [ 0, 1, 2, 3 ]
    ...SimdInstr(kExprI32x4Sub),
     // [
     //   (3 * (2 - 2)) - 0,
     //   (1 * (0 - 3)) - 1,
     //   (2 * (1 - 3)) - 2,
     //   (2 * (0 - 1)) - 3,
     // ]
    kExprLocalTee, 6,
    kSimdPrefix, kExprI32x4ExtractLane, 2,
    kExprLocalGet, 6,
    kSimdPrefix, kExprI32x4ExtractLane, 0,
    kExprI32Sub,
    kExprLocalGet, 6,
    kSimdPrefix, kExprI32x4ExtractLane, 1,
    kExprLocalGet, 6,
    kSimdPrefix, kExprI32x4ExtractLane, 3,
    kExprI32Sub,
    kExprI32Sub,
  ]);
  // lanes:
  //   0: -local_0
  //   1: ((local_0 - local_3) * local_1) - local_1
  //   2: ((local_1 - local_3) * local_2) - local_2
  //   3: ((local_1 - local_2) * local_2) - local_3
  const scalar = (local_0, local_1, local_2, local_3) => {
    const masked_local_0 = 0xFF & local_0;
    const masked_local_1 = 0xFF & local_1;
    const masked_local_2 = 0xFF & local_2;
    const masked_local_3 = 0xFF & local_3;
    const lane_0 = -masked_local_0;
    const lane_1 = ((masked_local_0 - masked_local_3) * masked_local_1) - masked_local_1;
    const lane_2 = ((masked_local_1 - masked_local_3) * masked_local_2) - masked_local_2;
    const lane_3 = ((masked_local_0 - masked_local_1) * masked_local_2) - masked_local_3;
    return (lane_2 - lane_0) - (lane_1 - lane_3);
  };
  builder.addFunction("simd", kSig_i_iiii).addLocals(kWasmS128, 3).addBody(simd).exportFunc();
  const wasm = builder.instantiate().exports;
  for (let i = 0; i < int8_array.length - 3; ++i) {
    const args = int8_array.slice(i, i + 3);
    assertEquals(wasm.simd(...args),
                 scalar(...args));
  }
})();

(function ShuffleS8x4MulTo64x2() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprI8x16Splat,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 1,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 2,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI8x16ReplaceLane, 3,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 5,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 6,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI8x16ReplaceLane, 7,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 9,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 10,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI8x16ReplaceLane, 11,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 13,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 14,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI8x16ReplaceLane, 15,
    kExprLocalTee, 4,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI8x16Splat,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 1,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 2,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI8x16ReplaceLane, 3,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 5,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 6,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI8x16ReplaceLane, 7,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 9,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 10,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI8x16ReplaceLane, 11,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI8x16ReplaceLane, 13,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16ReplaceLane, 14,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI8x16ReplaceLane, 15,
    kExprLocalTee, 5,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ...SimdInstr(kExprI16x8SConvertI8x16Low),
    kExprLocalGet, 4,
    kExprLocalGet, 5,
    kSimdPrefix, kExprI8x16Shuffle,
    0x1c, 0x1d, 0x1e, 0x1f, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ...SimdInstr(kExprI16x8SConvertI8x16Low),
    ...SimdInstr(kExprI32x4ExtMulLowI16x8S),
    ...SimdInstr(kExprI64x2SConvertI32x4Low),
    kExprLocalTee, 6,
    kSimdPrefix, kExprI64x2ExtractLane, 0,
    kExprLocalGet, 6,
    kSimdPrefix, kExprI64x2ExtractLane, 1,
    kExprI64Add,
  ];
  const scalar = [
    kExprLocalGet, 0,
    kExprI32SExtendI8,
    kExprLocalGet, 3,
    kExprI32SExtendI8,
    kExprI32Mul,
    kExprI64SConvertI32,
    kExprLocalGet, 1,
    kExprI32SExtendI8,
    kExprLocalGet, 2,
    kExprI32SExtendI8,
    kExprI32Mul,
    kExprI64SConvertI32,
    kExprI64Add,
  ];
  builder.addFunction("simd", kSig_l_iiii).addLocals(kWasmS128, 3).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_l_iiii).addBody(scalar).exportFunc();
  const wasm = builder.instantiate().exports;
  for (let i = 0; i < int8_array.length - 3; ++i) {
    const args = int8_array.slice(i, i + 3);
    assertEquals(wasm.simd(...args),
                 wasm.scalar(...args));
  }
})();

(function ShuffleS16x4MulLowTo32x4() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprI16x8Splat,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI16x8ReplaceLane, 1,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI16x8ReplaceLane, 2,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI16x8ReplaceLane, 3,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI16x8ReplaceLane, 5,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI16x8ReplaceLane, 6,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI16x8ReplaceLane, 7,
    kExprLocalTee, 4,
    kExprLocalGet, 4,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x04, 0x05, 0x08, 0x09, 0x0c, 0x0d,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    kExprLocalGet, 4,
    kExprLocalGet, 4,
    kSimdPrefix, kExprI8x16Shuffle,
    0x02, 0x03, 0x06, 0x07, 0x0a, 0x0b, 0x0e, 0x0f,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ...SimdInstr(kExprI32x4ExtMulLowI16x8S),
    kExprLocalTee, 5,
    kSimdPrefix, kExprI32x4ExtractLane, 0,
    kExprLocalGet, 5,
    kSimdPrefix, kExprI32x4ExtractLane, 3,
    kExprI32Sub
  ];
  const scalar = [
    kExprLocalGet, 0,
    kExprI32SExtendI16,
    kExprLocalGet, 1,
    kExprI32SExtendI16,
    kExprI32Mul,
    kExprLocalGet, 2,
    kExprI32SExtendI16,
    kExprLocalGet, 3,
    kExprI32SExtendI16,
    kExprI32Mul,
    kExprI32Sub,
  ];
  builder.addFunction("simd", kSig_i_iiii).addLocals(kWasmS128, 2).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_i_iiii).addBody(scalar).exportFunc();
  const wasm = builder.instantiate().exports;
  for (let i = 0; i < int16_array.length - 3; ++i) {
    const args = int16_array.slice(i, i + 3);
    assertEquals(wasm.simd(...args),
                 wasm.scalar(...args));
  }
})();

(function ShuffleS16x4ConvertHighLowTo32x4() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprI16x8Splat,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI16x8ReplaceLane, 1,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI16x8ReplaceLane, 2,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI16x8ReplaceLane, 3,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI16x8ReplaceLane, 5,
    kExprLocalGet, 2,
    kSimdPrefix, kExprI16x8ReplaceLane, 6,
    kExprLocalGet, 3,
    kSimdPrefix, kExprI16x8ReplaceLane, 7,
    kExprLocalTee, 4,
    kExprLocalGet, 4,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x01, 0x04, 0x05, 0x08, 0x09, 0x0c, 0x0d,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ...SimdInstr(kExprI32x4SConvertI16x8Low),
    kExprLocalGet, 4,
    kExprLocalGet, 4,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x03, 0x06, 0x07, 0x0a, 0x0b, 0x0e, 0x0f,
    ...SimdInstr(kExprI32x4SConvertI16x8High),
    ...SimdInstr(kExprS128Xor),
    kExprLocalTee, 5,
    kSimdPrefix, kExprI32x4ExtractLane, 0,
    kExprLocalGet, 5,
    kSimdPrefix, kExprI32x4ExtractLane, 3,
    kExprI32Sub
  ];
  const scalar = [
    kExprLocalGet, 0,
    kExprI32SExtendI16,
    kExprLocalGet, 1,
    kExprI32SExtendI16,
    kExprI32Xor,
    kExprLocalGet, 2,
    kExprI32SExtendI16,
    kExprLocalGet, 3,
    kExprI32SExtendI16,
    kExprI32Xor,
    kExprI32Sub,
  ];
  builder.addFunction("simd", kSig_i_iiii).addLocals(kWasmS128, 2).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_i_iiii).addBody(scalar).exportFunc();
  const wasm = builder.instantiate().exports;
  for (let i = 0; i < int16_array.length - 3; ++i) {
    const args = int16_array.slice(i, i + 3);
    assertEquals(wasm.simd(...args),
                 wasm.scalar(...args));
  }
})();

(function SameConvertChainToOneByte() {
  print(arguments.callee.name);
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprI8x16Splat,
    kExprLocalTee, 2,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16Splat,
    kExprLocalTee, 3,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x10, 0x01, 0x11, 0x02, 0x12, 0x03, 0x13,
    0x04, 0x14, 0x05, 0x15, 0x06, 0x16, 0x07, 0x17,
    ...SimdInstr(kExprI16x8SConvertI8x16Low),   // 0, 1, 2, 3, 4, 5, 6, 7
    ...SimdInstr(kExprI16x8SConvertI8x16Low),   // 0, 1, 2, 3
    ...SimdInstr(kExprI16x8SConvertI8x16Low),   // 0, 1,
    ...SimdInstr(kExprI16x8SConvertI8x16Low),   // 0,
    ...SimdInstr(kExprI16x8SConvertI8x16Low),   // 0,
    ...SimdInstr(kExprI16x8SConvertI8x16Low),   // 0,
    ...SimdInstr(kExprI16x8SConvertI8x16Low),   // 0,
    ...SimdInstr(kExprI16x8SConvertI8x16Low),   // 0,
    kSimdPrefix, kExprI16x8ExtractLaneS, 0,
  ];
  const scalar = [
    kExprLocalGet, 0,
    kExprI32SExtendI8,
  ];
  const builder = new WasmModuleBuilder();
  builder.addFunction("simd", kSig_i_ii).addLocals(kWasmS128, 4).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_i_ii).addBody(scalar).exportFunc();
  const wasm = builder.instantiate().exports;
  for (let i = 0; i < int8_array.length - 2; ++i) {
    const args = int8_array.slice(i, i + 2);
    assertEquals(wasm.simd(...args),
                 wasm.scalar(...args));
  }
})();

(function DifferentConvertChainToOneByte() {
  print(arguments.callee.name);
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprI8x16Splat,
    kExprLocalTee, 2,
    kExprLocalGet, 1,
    kSimdPrefix, kExprI8x16Splat,
    kExprLocalTee, 3,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x10, 0x01, 0x11, 0x02, 0x12, 0x03, 0x13,
    0x04, 0x14, 0x05, 0x15, 0x06, 0x16, 0x07, 0x17,
    ...SimdInstr(kExprI16x8SConvertI8x16Low),   // 0, 1, 2, 3, 4, 5, 6, 7
    ...SimdInstr(kExprI32x4UConvertI16x8Low),   // 0, 1, 2, 3
    ...SimdInstr(kExprI32x4UConvertI16x8Low),   // 0, 1,
    ...SimdInstr(kExprI16x8UConvertI8x16Low),   // 0,
    ...SimdInstr(kExprI64x2SConvertI32x4Low),   // 0,
    ...SimdInstr(kExprI16x8UConvertI8x16Low),   // 0,
    kSimdPrefix, kExprI16x8ExtractLaneU, 0,
  ];
  const scalar = [
    kExprLocalGet, 0,
    ...wasmI32Const(0xFF),
    kExprI32And,
  ];
  const builder = new WasmModuleBuilder();
  builder.addFunction("simd", kSig_i_ii).addLocals(kWasmS128, 4).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_i_ii).addBody(scalar).exportFunc();
  const wasm = builder.instantiate().exports;
  for (let i = 0; i < int8_array.length - 2; ++i) {
    const args = int8_array.slice(i, i + 2);
    assertEquals(wasm.simd(...args),
                 wasm.scalar(...args));
  }
})();

(function Crash462259483() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const simd = [
    kExprLocalGet, 0,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalTee, 1,
    kExprLocalGet, 1,
    ...SimdInstr(kExprI32x4ExtMulLowI16x8U),
    kExprLocalTee, 2,
    kExprLocalGet, 2,
    ...SimdInstr(kExprI32x4ExtMulLowI16x8U),
    kExprLocalTee, 3,
    kExprLocalGet, 1,
    ...SimdInstr(kExprI32x4ExtMulLowI16x8U),
    kExprLocalSet, 4,
    kExprLocalGet, 1,
    kExprLocalGet, 1,
    ...SimdInstr(kExprI32x4ExtMulLowI16x8U),
    kExprLocalTee, 5,
    kExprLocalGet, 1,
    ...SimdInstr(kExprI32x4ExtMulLowI16x8U),
    kExprLocalSet, 6,
    kExprLocalGet, 1,
    kExprLocalGet, 1,
    ...SimdInstr(kExprI32x4ExtMulLowI16x8U),
    kExprLocalTee, 7,
    kExprLocalGet, 1,
    ...SimdInstr(kExprI32x4ExtMulLowI16x8U),
    kExprLocalTee, 8,
    kExprLocalGet, 1,
    ...SimdInstr(kExprI32x4ExtMulLowI16x8U),
    kExprLocalSet, 9,
    kExprLocalGet, 1,
    kExprLocalGet, 1,
    ...SimdInstr(kExprI32x4ExtMulLowI16x8U),
    kExprLocalTee, 10,
    kExprLocalGet, 1,
    ...SimdInstr(kExprI32x4ExtMulLowI16x8U),
    kExprLocalTee, 11,
    kExprLocalGet, 9,
    ...SimdInstr(kExprI32x4ExtMulLowI16x8U),
    kExprLocalTee, 12,
    kExprLocalGet, 1,
    ...SimdInstr(kExprI32x4ExtMulLowI16x8U),
    kExprLocalTee, 13,
    kExprLocalGet, 6,
    ...SimdInstr(kExprI32x4ExtMulLowI16x8U),
    kExprLocalTee, 14,
    kExprLocalGet, 1,
    ...SimdInstr(kExprI32x4ExtMulLowI16x8U),
    kExprLocalTee, 15,
    kExprLocalGet, 4,
    ...SimdInstr(kExprI32x4ExtMulLowI16x8U),
    kExprLocalTee, 16,
    kSimdPrefix, kExprI8x16BitMask,
  ];
  builder.addFunction("simd", kSig_i_i).addLocals(kWasmS128, 16).addBody(simd).exportFunc();
  const wasm = builder.instantiate().exports;
  assertEquals(wasm.simd(0), 0);
})();
