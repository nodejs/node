// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

(function ExtMulLowWithTwoExtracts() {
  print(arguments.callee.name);

  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.exportMemoryAs('memory');
  builder.addFunction('extract', kSig_i_ii).addLocals(kWasmS128, 3)
      .addBody([
        kExprLocalGet, 0,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprLocalTee, 2,
        kExprLocalGet, 1,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprLocalTee, 3,
        kSimdPrefix, kExprI8x16Shuffle,
        8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23,
        kExprLocalGet, 2,
        kExprLocalGet, 3,
        kSimdPrefix, kExprI8x16Shuffle,
        8, 9, 10, 11, 12, 13, 14, 15,
        0, 1, 2, 3, 4, 5, 6, 7,
        // ExtMulLow on the two shuffled vectors.
        ...SimdInstr(kExprI16x8ExtMulLowI8x16S),
        kExprLocalTee, 4,
        // Two lane-0 extracts of the ExtMulLow result.
        kSimdPrefix, kExprI16x8ExtractLaneS, 0,
        kExprLocalGet, 4,
        kSimdPrefix, kExprI16x8ExtractLaneS, 0,
        kExprI32Add,
      ])
      .exportFunc();

  builder.addFunction('scalar', kSig_i_ii).addLocals(kWasmI32, 1)
      .addBody([
        kExprLocalGet, 0,
        kExprI32LoadMem8S, 0, 8,
        kExprLocalTee, 2,
        kExprLocalGet, 2,
        kExprI32Mul,
        kExprI32Const, 2,
        kExprI32Mul,
      ])
      .exportFunc();

  const wasm = builder.instantiate().exports;
  const mem = new Uint8Array(wasm.memory.buffer);
  const base0 = 0;
  const base1 = 32;
  for (let i = 0; i < 64; ++i) {
    mem[i] = int8_array[i % int8_array.length];
  }

  assertEquals(wasm.scalar(base0, base1), wasm.extract(base0, base1));
})();

(function StoreLaneDemandedBytes() {
  print(arguments.callee.name);

  const left = [
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  ];
  const right = [
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  ];

  const shuffle = [
    20, 5, 21, 4, 22, 3, 23, 2,
    0, 1, 2, 3, 4, 5, 6, 7,
  ];

  const expectedLane = function(bits) {
    const source = new Uint8Array([...left, ...right]);
    const out = new Uint8Array(16);
    for (let i = 0; i < 16; ++i) out[i] = source[shuffle[i]];
    const view = new DataView(out.buffer);
    switch (bits) {
      case 8:
        return out[0];
      case 16:
        return view.getUint16(0, true);
      case 32:
        return view.getUint32(0, true);
      case 64:
        return view.getBigUint64(0, true);
      default:
        throw new Error('Unsupported lane width');
    }
  }

  const makeStoreLaneBody = function(storeOpcode, loadOpcode) {
    return [
      ...wasmI32Const(0),
      ...wasmS128Const(left),
      ...wasmS128Const(right),
      kSimdPrefix, kExprI8x16Shuffle, ...shuffle,
      kSimdPrefix, storeOpcode, 0, 0, 0,
      ...wasmI32Const(0),
      loadOpcode, 0, 0,
    ];
  }

  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1);

  builder.addFunction('store8', kSig_i_v)
      .addBody(makeStoreLaneBody(kExprS128Store8Lane, kExprI32LoadMem8U))
      .exportFunc();

  builder.addFunction('store16', kSig_i_v)
      .addBody(makeStoreLaneBody(kExprS128Store16Lane, kExprI32LoadMem16U))
      .exportFunc();

  builder.addFunction('store32', kSig_i_v)
      .addBody(makeStoreLaneBody(kExprS128Store32Lane, kExprI32LoadMem))
      .exportFunc();

  builder.addFunction('store64', kSig_l_v)
      .addBody(makeStoreLaneBody(kExprS128Store64Lane, kExprI64LoadMem))
      .exportFunc();

  const wasm = builder.instantiate().exports;

  assertEquals(expectedLane(8), wasm.store8());
  assertEquals(expectedLane(16), wasm.store16());
  assertEquals(expectedLane(32), wasm.store32());
  assertEquals(expectedLane(64), wasm.store64());
})();

function RunOneShuffleMaxIndexTest(name, outerShuffle) {
  print(name);
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.exportMemoryAs('memory');

  const innerShuffle = [
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 10, 11, 12, 13, 14, 15,
  ];

  builder.addFunction('one_input_shuffle', makeSig(
    [kWasmI32, kWasmI32, kWasmI32], [])).addLocals(kWasmS128, 1)
      .addBody([
        kExprLocalGet, 2,
        kExprLocalGet, 0,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprLocalGet, 1,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kSimdPrefix, kExprI8x16Shuffle, ...innerShuffle,
        kExprLocalTee, 3,
        kExprLocalGet, 3,
        kSimdPrefix, kExprI8x16Shuffle, ...outerShuffle,
        kSimdPrefix, kExprS128StoreMem, 0, 0,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const memory = new Uint8Array(instance.exports.memory.buffer);
  const leftAddr = 0;
  const rightAddr = 16;
  const dstAddr = 64;

  const srcAddrs = [leftAddr, rightAddr];
  for (let i = 0; i < dstAddr; ++i) {
    memory[i] = int8_array[i % int8_array.length];
  }
  memory.fill(0, dstAddr, dstAddr + 16);

  instance.exports.one_input_shuffle(leftAddr, rightAddr, dstAddr);

  const applyShuffle = (input, shuf) => Uint8Array.from(shuf.map(i => input[i]));
  const input = memory.slice(leftAddr, leftAddr + 32);
  const innerResult = applyShuffle(input, innerShuffle);

  const expected = new Uint8Array(16);
  for (let i = 0; i < 16; ++i) {
    const idx = outerShuffle[i];
    expected[i] = innerResult[idx % 16];
  }

  for (let i = 0; i < 16; ++i) {
    assertEquals(expected[i], memory[dstAddr + i]);
  }
}

function RunTwoShufflesMaxIndexTest(name, outerShuffle) {
  print(name);
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.exportMemoryAs('memory');

  const innerShuffle = [
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 10, 11, 12, 13, 14, 15,
  ];

  builder.addFunction('two_input_shuffles', makeSig(
    [kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 4,
        kExprLocalGet, 0,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprLocalGet, 1,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kSimdPrefix, kExprI8x16Shuffle, ...innerShuffle,
        kExprLocalGet, 2,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprLocalGet, 3,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kSimdPrefix, kExprI8x16Shuffle, ...innerShuffle,
        kSimdPrefix, kExprI8x16Shuffle, ...outerShuffle,
        kSimdPrefix, kExprS128StoreMem, 0, 0,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const memory = new Uint8Array(instance.exports.memory.buffer);
  const leftAAddr = 0;
  const rightAAddr = 16;
  const leftBAddr = 32;
  const rightBAddr = 48;
  const dstAddr = 64;

  const srcAddrs = [leftAAddr, rightAAddr, leftBAddr, rightBAddr];
  for (let i = 0; i < dstAddr; ++i) {
    memory[i] = int8_array[i % int8_array.length];
  }
  memory.fill(0, dstAddr, dstAddr + 16);

  instance.exports.two_input_shuffles(
      leftAAddr, rightAAddr, leftBAddr, rightBAddr, dstAddr);

  const applyShuffle = (input, shuf) => Uint8Array.from(shuf.map(i => input[i]));
  const inputA = memory.slice(leftAAddr, leftAAddr + 32);
  const inputB = memory.slice(leftBAddr, leftBAddr + 32);
  const innerResultA = applyShuffle(inputA, innerShuffle);
  const innerResultB = applyShuffle(inputB, innerShuffle);

  const expected = new Uint8Array(16);
  for (let i = 0; i < 16; ++i) {
    const idx = outerShuffle[i];
    expected[i] = idx < 16 ? innerResultA[idx] : innerResultB[idx - 16];
  }

  for (let i = 0; i < 16; ++i) {
    assertEquals(expected[i], memory[dstAddr + i]);
  }
}

function RunMaxIndexTests(name, outerShuffle) {
  RunOneShuffleMaxIndexTest(name, outerShuffle);
  RunTwoShufflesMaxIndexTest(name, outerShuffle);
}

// Both inner shuffles should be reduced to I8x2 (max index 1/17).
RunMaxIndexTests(
    'ShuffleMaxIndex_ReduceToI8x2_Positive',
    [
      0, 16, 1, 17,
      0, 16, 1, 17,
      0, 16, 1, 17,
      0, 16, 1, 17,
    ]);

// Both inner shuffles should be reduced to I8x4 (max index 3/19).
RunMaxIndexTests(
    'ShuffleMaxIndex_ReduceToI8x4_Positive',
    [
      0, 16, 1, 17,
      2, 18, 3, 19,
      0, 16, 1, 17,
      2, 18, 3, 19,
    ]);

// Both inner shuffles should be reduced to I8x8 (max index 7/23).
RunMaxIndexTests(
    'ShuffleMaxIndex_ReduceToI8x8_Positive',
    [
      0, 16, 1, 17,
      2, 18, 3, 19,
      4, 20, 5, 21,
      6, 22, 7, 23,
    ]);

// Includes index 2/18, so can be reduced to I8x4.
RunMaxIndexTests(
    'ShuffleMaxIndex_NoReduceToI8x2_Negative',
    [
      0, 16, 1, 17,
      2, 18, 0, 16,
      1, 17, 2, 18,
      0, 16, 1, 17,
    ]);

// Includes index 4/20, so can be reduced to I8x8.
RunMaxIndexTests(
    'ShuffleMaxIndex_NoReduceToI8x4_Negative',
    [
      0, 16, 1, 17,
      2, 18, 3, 19,
      4, 20, 2, 18,
      3, 19, 0, 16,
    ]);

// Includes index 8/24, so it won't be reduced.
RunMaxIndexTests(
    'ShuffleMaxIndex_NoReduceToI8x8_Negative',
    [
      0, 16, 1, 17,
      2, 18, 3, 19,
      4, 20, 5, 21,
      6, 22, 8, 24,
    ]);

(function TwoByteShuffleOfShuffleLowLanes() {
  print(arguments.callee.name);

  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('simd', kSig_l_v).addLocals(kWasmS128, 1)
      .addBody([
        kExprI32Const, 0,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprI32Const, 16,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kSimdPrefix, kExprI8x16Shuffle,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        kExprI32Const, 32,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kSimdPrefix, kExprI8x16Shuffle,
        12, 16, 17, 18, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
        // Chain low-half, so we only need the bottom two bytes of the shuffle.
        // Lane 12 comes from mem[12]
        // Lane 16 comes from mem[32]
        ...SimdInstr(kExprI16x8SConvertI8x16Low),
        ...SimdInstr(kExprI32x4SConvertI16x8Low),
        ...SimdInstr(kExprI64x2SConvertI32x4Low),
        kExprLocalTee, 0,
        kSimdPrefix, kExprI64x2ExtractLane, 0,
        kExprLocalGet, 0,
        kSimdPrefix, kExprI64x2ExtractLane, 1,
        kExprI64Sub,
      ])
      .exportFunc();

  builder.addFunction('scalar', kSig_l_v)
      .addBody([
        kExprI32Const, 12,
        kExprI64LoadMem8S, 0, 0,
        kExprI32Const, 32,
        kExprI64LoadMem8S, 0, 0,
        kExprI64Sub,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const mem = new Uint8Array(instance.exports.memory.buffer);
  for (let i = 0; i < 48; ++i) {
    mem[i] = int8_array[i % int8_array.length];
  }

  assertEquals(instance.exports.scalar(), instance.exports.simd());
})();

(function FourByteShuffleOfShuffleLowLanes() {
  print(arguments.callee.name);

  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('simd', kSig_l_v).addLocals(kWasmS128, 1)
      .addBody([
        kExprI32Const, 0,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprI32Const, 16,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kSimdPrefix, kExprI8x16Shuffle,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        kExprI32Const, 32,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kSimdPrefix, kExprI8x16Shuffle,
        12, 13, 17, 18, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
        // Chain low-half, so we only need the bottom four bytes of the shuffle.
        // Lane 12 comes from mem[16 + 12]
        // Lane 13 comes from mem[16 + 13]
        // Lane 17 comes from mem[32 + 1]
        // Lane 18 comes from mem[32 + 2]
        ...SimdInstr(kExprI32x4SConvertI16x8Low),
        ...SimdInstr(kExprI64x2SConvertI32x4Low),
        kExprLocalTee, 0,
        kSimdPrefix, kExprI64x2ExtractLane, 0,
        kExprLocalGet, 0,
        kSimdPrefix, kExprI64x2ExtractLane, 1,
        kExprI64Sub,
      ])
      .exportFunc();

  builder.addFunction('scalar', kSig_l_v)
      .addBody([
        kExprI32Const, 28,
        kExprI64LoadMem16S, 0, 0,
        kExprI32Const, 33,
        kExprI64LoadMem16S, 0, 0,
        kExprI64Sub,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const mem = new Uint8Array(instance.exports.memory.buffer);
  for (let i = 0; i < 48; ++i) {
    mem[i] = int8_array[i % int8_array.length];
  }

  assertEquals(instance.exports.scalar(), instance.exports.simd());
})();

(function FourByteShuffleOfShuffleLowLanesReverseHalfWords() {
  print(arguments.callee.name);

  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('simd', kSig_l_v).addLocals(kWasmS128, 1)
      .addBody([
        kExprI32Const, 0,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprI32Const, 16,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kSimdPrefix, kExprI8x16Shuffle,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        kExprI32Const, 32,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kSimdPrefix, kExprI8x16Shuffle,
        13, 12, 18, 17, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
        // Chain low-half, so we only need the bottom four bytes of the shuffle.
        // Lane 13 comes from mem[16 + 13]
        // Lane 12 comes from mem[16 + 12]
        // Lane 18 comes from mem[32 + 2]
        // Lane 17 comes from mem[32 + 1]
        ...SimdInstr(kExprI32x4UConvertI16x8Low),
        ...SimdInstr(kExprI64x2UConvertI32x4Low),
        kExprLocalTee, 0,
        kSimdPrefix, kExprI64x2ExtractLane, 0,
        kExprLocalGet, 0,
        kSimdPrefix, kExprI64x2ExtractLane, 1,
        kExprI64Sub,
      ])
      .exportFunc();

  builder.addFunction('scalar', kSig_l_v)
      .addBody([
        kExprI32Const, 29,
        kExprI64LoadMem8U, 0, 0,
        kExprI32Const, 28,
        kExprI64LoadMem8U, 0, 0,
        kExprI64Const, 8,
        kExprI64Shl,
        kExprI64Ior,
        kExprI32Const, 34,
        kExprI64LoadMem8U, 0, 0,
        kExprI32Const, 33,
        kExprI64LoadMem8U, 0, 0,
        kExprI64Const, 8,
        kExprI64Shl,
        kExprI64Ior,
        kExprI64Sub,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const mem = new Uint8Array(instance.exports.memory.buffer);
  for (let i = 0; i < 48; ++i) {
    mem[i] = int8_array[i % int8_array.length];
  }

  assertEquals(instance.exports.scalar(), instance.exports.simd());
})();

(function FourByteShuffleOfShuffleLowLanesReverseWord() {
  print(arguments.callee.name);

  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('simd', kSig_l_v).addLocals(kWasmS128, 1)
      .addBody([
        kExprI32Const, 0,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprI32Const, 16,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kSimdPrefix, kExprI8x16Shuffle,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        kExprI32Const, 32,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kSimdPrefix, kExprI8x16Shuffle,
        14, 13, 12, 11, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
        // Chain low-half, so we only need the bottom four bytes of the shuffle.
        // Lane 14 comes from mem[16 + 14]
        // Lane 13 comes from mem[16 + 13]
        // Lane 12 comes from mem[16 + 12]
        // Lane 11 comes from mem[16 + 11]
        ...SimdInstr(kExprI32x4UConvertI16x8Low),
        ...SimdInstr(kExprI64x2UConvertI32x4Low),
        kExprLocalTee, 0,
        kSimdPrefix, kExprI64x2ExtractLane, 0,
        kExprLocalGet, 0,
        kSimdPrefix, kExprI64x2ExtractLane, 1,
        kExprI64Sub,
      ])
      .exportFunc();

  builder.addFunction('scalar', kSig_l_v)
      .addBody([
        kExprI32Const, 30,
        kExprI64LoadMem8U, 0, 0,
        kExprI32Const, 29,
        kExprI64LoadMem8U, 0, 0,
        kExprI64Const, 8,
        kExprI64Shl,
        kExprI64Ior,
        kExprI32Const, 28,
        kExprI64LoadMem8U, 0, 0,
        kExprI32Const, 27,
        kExprI64LoadMem8U, 0, 0,
        kExprI64Const, 8,
        kExprI64Shl,
        kExprI64Ior,
        kExprI64Sub,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const mem = new Uint8Array(instance.exports.memory.buffer);
  for (let i = 0; i < 48; ++i) {
    mem[i] = int8_array[i % int8_array.length];
  }

  assertEquals(instance.exports.scalar(), instance.exports.simd());
})();

(function FourByteShuffleOfShuffleLowLanesInterleavedWord() {
  print(arguments.callee.name);

  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('simd', kSig_l_v).addLocals(kWasmS128, 1)
      .addBody([
        kExprI32Const, 32,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprI32Const, 0,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprI32Const, 16,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kSimdPrefix, kExprI8x16Shuffle,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        kSimdPrefix, kExprI8x16Shuffle,
        22, 20, 18, 16, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
        // Chain low-half, so we only need the bottom four bytes of the shuffle.
        // Lane 22 comes from mem[16 + 6]
        // Lane 20 comes from mem[16 + 4]
        // Lane 18 comes from mem[16 + 2]
        // Lane 16 comes from mem[16 + 0]
        ...SimdInstr(kExprI32x4UConvertI16x8Low),
        ...SimdInstr(kExprI64x2UConvertI32x4Low),
        kExprLocalTee, 0,
        kSimdPrefix, kExprI64x2ExtractLane, 0,
        kExprLocalGet, 0,
        kSimdPrefix, kExprI64x2ExtractLane, 1,
        kExprI64Sub,
      ])
      .exportFunc();

  builder.addFunction('scalar', kSig_l_v)
      .addBody([
        kExprI32Const, 22,
        kExprI64LoadMem8U, 0, 0,
        kExprI32Const, 20,
        kExprI64LoadMem8U, 0, 0,
        kExprI64Const, 8,
        kExprI64Shl,
        kExprI64Ior,
        kExprI32Const, 18,
        kExprI64LoadMem8U, 0, 0,
        kExprI32Const, 16,
        kExprI64LoadMem8U, 0, 0,
        kExprI64Const, 8,
        kExprI64Shl,
        kExprI64Ior,
        kExprI64Sub,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const mem = new Uint8Array(instance.exports.memory.buffer);
  for (let i = 0; i < 48; ++i) {
    mem[i] = int8_array[i % int8_array.length];
  }

  assertEquals(instance.exports.scalar(), instance.exports.simd());
})();

(function MultipleUsedRanges() {
  print(arguments.callee.name);

  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('simd', kSig_i_v).addLocals(kWasmS128, 1)
      .addBody([
        kExprI32Const, 0,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprI32Const, 16,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kSimdPrefix, kExprI8x16Shuffle,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        kExprI32Const, 32,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kSimdPrefix, kExprI8x16Shuffle,
        // multiple disjoint ranges of the incoming shuffle.
        4, 5, 16, 17, 8, 9, 18, 19,
        24, 25, 20, 21, 0, 1, 22, 23,
        ...SimdInstr(kExprI16x8SConvertI8x16Low),
        kExprLocalTee, 0,
        kSimdPrefix, kExprI16x8ExtractLaneS, 0,
        kExprLocalGet, 0,
        kSimdPrefix, kExprI16x8ExtractLaneS, 4,
        kExprI32Sub,
      ])
      .exportFunc();

  builder.addFunction('scalar', kSig_i_v)
      .addBody([
        kExprI32Const, 4,
        kExprI32LoadMem8S, 0, 0,
        kExprI32Const, 8,
        kExprI32LoadMem8S, 0, 0,
        kExprI32Sub,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const mem = new Uint8Array(instance.exports.memory.buffer);
  for (let i = 0; i < 48; ++i) {
    mem[i] = int8_array[i % int8_array.length];
  }

  assertEquals(instance.exports.scalar(), instance.exports.simd());
})();

(function TestDisjointWindowsAreNotReduced() {
  const builder = new WasmModuleBuilder();

  const identity = [...Array(16).keys()];
  const outer = [
    0, 1, 2, 3,
    16, 17, 18, 19,
    4, 5, 6, 7,
    16, 17, 18, 19
  ];

  // inner = [0, 1, 2, ... 15]
  // outer keeps the first block of inner in lanes 0-3 and another block in
  // lanes 8-11. Reducing the inner shuffle would move the second block and
  // corrupt lanes 0-3; the fix adds a pre-window check to prevent that.
  builder.addFunction('disjoint', kSig_i_v)
      .addLocals(kWasmS128, 1)
      .addBody([
        ...wasmS128Const(identity),                // left for inner
        ...wasmS128Const(identity),                // right for inner
        kSimdPrefix, kExprI8x16Shuffle, ...identity,

        ...wasmS128Const(Array.from({length: 16}, (_, i) => 200 + i)),
        kSimdPrefix, kExprI8x16Shuffle, ...outer,

        kExprLocalSet, 0,
        kExprLocalGet, 0,
        kSimdPrefix, kExprI8x16ExtractLaneU, 8,
        kExprLocalGet, 0,
        kSimdPrefix, kExprI8x16ExtractLaneU, 0,
        kExprI32Sub,
      ])
      .exportFunc();

  const wasm = builder.instantiate().exports;
  assertEquals(4, wasm.disjoint());
})();

(function TestRepeatedIndexWindowRewritesAllDemandedBytes() {
  const builder = new WasmModuleBuilder();

  const identity = [...Array(16).keys()];
  const hot = new Array(16).fill(0);
  hot[8] = 99;  // Unique byte that must be replicated into the low half.

  const repeatedIndex = [
    8, 8, 8, 8, 8, 8, 8, 8,  // demanded bytes (low half)
    16, 17, 18, 19, 20, 21, 22, 23
  ];

  builder.addFunction('repeated', kSig_i_v)
      .addLocals(kWasmS128, 1)
      .addBody([
        ...wasmS128Const(hot),                      // left for inner
        ...wasmS128Const(new Array(16).fill(0)),    // right for inner
        kSimdPrefix, kExprI8x16Shuffle, ...identity,

        ...wasmS128Const(new Array(16).fill(0)),
        kSimdPrefix, kExprI8x16Shuffle, ...repeatedIndex,

        kExprLocalSet, 0,
        kExprLocalGet, 0,
        kSimdPrefix, kExprI8x16ExtractLaneU, 7,
      ])
      .exportFunc();

  const wasm = builder.instantiate().exports;
  assertEquals(99, wasm.repeated());
})();
