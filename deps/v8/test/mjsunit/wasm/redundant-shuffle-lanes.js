// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff
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
  for (let a of int32_array) {
    for (let b of int32_array) {
      assertEquals(wasm.simd(b, b, a, a),
                   wasm.scalar(b, b, a, a));
    }
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
  for (let a of int32_array) {
    for (let b of int32_array) {
      assertEquals(wasm.simd(b, a, a, b),
                   wasm.scalar(b, a, a, b));
    }
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
  for (let a of int32_array) {
    for (let b of int32_array) {
      assertEquals(wasm.simd(b, a, b, a),
                   wasm.scalar(b, a, b, a));
    }
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
  for (let a of int32_array) {
    for (let b of int32_array) {
      assertEquals(wasm.simd(a, a, a, b),
                   wasm.scalar(a, a, a, b));
    }
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
  for (let a of int32_array) {
    for (let b of int32_array) {
      assertEquals(wasm.simd(b, b, a, a),
                   wasm.scalar(b, b, a, a));
    }
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
  for (let a of int32_array) {
    for (let b of int32_array) {
      assertEquals(wasm.simd(a, a, a, b),
                   wasm.scalar(a, a, a, b));
    }
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
  for (let a of int32_array) {
    for (let b of int32_array) {
      assertEquals(wasm.simd(b, a, a, b),
                   wasm.scalar(b, a, a, b));
    }
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
  for (let a of float64_array) {
    for (let b of float64_array) {
      assertEquals(wasm.simd(a, b, b, a),
                   wasm.scalar(a, b, b, a));
    }
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
  for (let a of int32_array) {
    for (let b of int32_array) {
      assertEquals(wasm.simd(a, b, b, a),
                   wasm.scalar(a, b, b, a));
    }
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
  for (let a of int8_array) {
    for (let b of int8_array) {
      for (let c of int8_array) {
        for (let d of int8_array) {
          assertEquals(wasm.simd(a, b, c, d),
                       wasm.scalar(a, b, c, d));
        }
      }
    }
  }
})();

(function InterleaveU8x4Add() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const simd = wasmI32Const(0x0).concat([
    kSimdPrefix, kExprI8x16Splat,
    kExprLocalTee, 5,
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
    kExprLocalGet, 4,
    kSimdPrefix, kExprI8x16Shuffle,
    0x03, 0x07, 0x0b, 0x0f, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ...SimdInstr(kExprI16x8UConvertI8x16Low),
    ...SimdInstr(kExprI32x4UConvertI16x8Low),
    ...SimdInstr(kExprI32x4Add),
    kExprLocalGet, 4,
    kExprLocalGet, 4,
    kSimdPrefix, kExprI8x16Shuffle,
    0x02, 0x06, 0x0a, 0x0e, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ...SimdInstr(kExprI16x8UConvertI8x16Low),
    ...SimdInstr(kExprI32x4UConvertI16x8Low),
    ...SimdInstr(kExprI32x4Add),
    kExprLocalGet, 4,
    kExprLocalGet, 4,
    kSimdPrefix, kExprI8x16Shuffle,
    0x01, 0x05, 0x09, 0x0d, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ...SimdInstr(kExprI16x8UConvertI8x16Low),
    ...SimdInstr(kExprI32x4UConvertI16x8Low),
    ...SimdInstr(kExprI32x4Add),
    kExprLocalGet, 4,
    kExprLocalGet, 4,
    kSimdPrefix, kExprI8x16Shuffle,
    0x00, 0x04, 0x08, 0x0c, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ...SimdInstr(kExprI16x8UConvertI8x16Low),
    ...SimdInstr(kExprI32x4UConvertI16x8Low),
    ...SimdInstr(kExprI32x4Add),
    kSimdPrefix, kExprI32x4ExtractLane, 0,
  ]);
  const byte_mask = wasmI32Const(0xFF);
  const scalar = wasmI32Const(0xFF).concat([
    kExprLocalTee, 4,
    kExprLocalGet, 0,
    kExprI32And,
    kExprLocalGet, 1,
    kExprLocalGet, 4,
    kExprI32And,
    kExprI32Add,
    kExprLocalGet, 2,
    kExprLocalGet, 4,
    kExprI32And,
    kExprLocalGet, 3,
    kExprLocalGet, 4,
    kExprI32And,
    kExprI32Add,
    kExprI32Add,
  ]);
  builder.addFunction("simd", kSig_i_iiii).addLocals(kWasmS128, 2).addBody(simd).exportFunc();
  builder.addFunction("scalar", kSig_i_iiii).addLocals(kWasmI32, 1).addBody(scalar).exportFunc();
  const wasm = builder.instantiate().exports;
  for (let a of int8_array) {
    for (let b of int8_array) {
      for (let c of int8_array) {
        for (let d of int8_array) {
          assertEquals(wasm.simd(a, b, c, d),
                       wasm.scalar(a, b, c, d));
        }
      }
    }
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
  for (let a of int8_array) {
    for (let b of int8_array) {
      for (let c of int8_array) {
        for (let d of int8_array) {
          assertEquals(wasm.simd(a, b, c, d),
                       wasm.scalar(a, b, c, d));
        }
      }
    }
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
  for (let a of int16_array) {
    for (let b of int16_array) {
      for (let c of int16_array) {
        for (let d of int16_array) {
          assertEquals(wasm.simd(a, b, c, d),
                       wasm.scalar(a, b, c, d));
        }
      }
    }
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
  for (let a of int16_array) {
    for (let b of int16_array) {
      for (let c of int16_array) {
        for (let d of int16_array) {
          assertEquals(wasm.simd(a, b, c, d),
                       wasm.scalar(a, b, c, d));
        }
      }
    }
  }
})();
