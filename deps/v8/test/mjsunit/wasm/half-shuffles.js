// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-simd-opt

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/value-helper.js');

// These tests check that an i8x8 shuffle gives the expected result.
// To construct an i8x8 shuffle, we use an i8x16 shuffle with a lower-half
// widening conversion. `wasm-shuffle-reducer` will recognise that only the
// lower half is used and so converts the i8x16 shuffle to an i8x8 shuffle.
//
// For each test case, two Wasm functions are used:
// 1. `simd`: Shuffles two constant vectors whose values are just their indices.
// 2. `expected`: Directly constructs the expected result of the shuffle by
//                creating a vector with the same indices as the shuffle.
// Both functions use the same extraction and conversion instructions.

const val01 = [0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
               0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f];
const val02 = [0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
               0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f];
const dummy = [0, 0, 0, 0, 0, 0, 0, 0];

// Test helper. The config object should contain:
// - name: Name of the canonical shuffle being tested. Where the shuffle tested
//         is a full-width canonical shuffle, suffix with "_LowerHalf".
// - shuffle: The shuffle indices to use in the lower half of the i8x16 shuffle.
// - convert: The conversion instruction which takes the lower half of the
//            i8x16 shuffle result and widens it.
// - epilogue: The operations which follow the shuffle which perform any
//             necessary conversions, extract the lanes, and combines them into
//             a single result.
// - sig: The signature of the `simd` and `expected` functions.
function ShuffleTest(config) {
  print(config.name);
  const builder = new WasmModuleBuilder();
  const simd = [
    ...wasmS128Const(val01),
    ...wasmS128Const(val02),
    kSimdPrefix, kExprI8x16Shuffle,
    ...config.shuffle,
    ...dummy,
    ...config.epilogue,
  ];
  const expected = [
    ...wasmS128Const([...config.shuffle, ...dummy]),
    ...config.epilogue,
  ];
  builder.addFunction("simd", config.sig)
         .addLocals(kWasmS128, 1).addBody(simd).exportFunc();
  builder.addFunction("expected", config.sig)
         .addLocals(kWasmS128, 1).addBody(expected).exportFunc();
  const wasm = builder.instantiate().exports;
  assertEquals(wasm.simd(), wasm.expected());
}

const shuffle_64x1_tests = [
  {
    name: "64x1Identity",
    shuffle: [0, 1, 2, 3, 4, 5, 6, 7],
  },
  {
    name: "64x2ReverseBytes_LowerHalf",
    shuffle: [7, 6, 5, 4, 3, 2, 1, 0],
  },
  {
    name: "64x2Odd_LowerHalf",
    shuffle: [8, 9, 10, 11, 12, 13, 14, 15],
  }
];

(function Shuffle64x1TestAll() {
  const epilogue = [
    kSimdPrefix, kExprI64x2ExtractLane, 0,
  ];
  for (const config of shuffle_64x1_tests) {
    ShuffleTest({
      ...config,
      epilogue: epilogue,
      sig: kSig_l_v,
    });
  }
})();

const shuffle_32x2_tests = [
  {
    name: "32x4Even_LowerHalf",
    shuffle: [0, 1, 2, 3, 8, 9, 10, 11],
  },
  {
    name: "32x4Odd_LowerHalf",
    shuffle: [4, 5, 6, 7, 12, 13, 14, 15],
  },
  {
    name: "32x4InterleaveLowHalves_LowerHalf",
    shuffle: [0, 1, 2, 3, 16, 17, 18, 19],
  },
  {
    name: "32x4InterleaveHighHalves_LowerHalf",
    shuffle: [8, 9, 10, 11, 24, 25, 26, 27],
  },
  {
    name: "32x4TransposeOdd_LowerHalf",
    shuffle: [4, 5, 6, 7, 20, 21, 22, 23],
  },
  {
    name: "32x4ReverseBytes_LowerHalf",
    shuffle: [3, 2, 1, 0, 7, 6, 5, 4],
  },
  {
    name: "32x4Reverse_LowerHalf",
    shuffle: [12, 13, 14, 15, 8, 9, 10, 11],
  },
  {
    name: "32x2Reverse_LowerHalf",
    shuffle: [4, 5, 6, 7, 0, 1, 2, 3],
  },
  {
    name: "32x4DupLow_Lane0_Input0",
    shuffle: [0, 1, 2, 3, 0, 1, 2, 3],
  },
  {
    name: "32x4DupLow_Lane1_Input1",
    shuffle: [20, 21, 22, 23, 20, 21, 22, 23],
  }
];

(function Shuffle32x2TestAll() {
  const epilogue = [
    ...SimdInstr(kExprI64x2SConvertI32x4Low),
    kExprLocalTee, 0,
    kSimdPrefix, kExprI64x2ExtractLane, 0,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI64x2ExtractLane, 1,
    kExprI64Sub,
  ];
  for (const config of shuffle_32x2_tests) {
    ShuffleTest({
      ...config,
      epilogue: epilogue,
      sig: kSig_l_v,
    });
  }
})();

const shuffle_16x4_tests = [
  {
    name: "16x4Even",
    shuffle: [0, 1, 4, 5, 16, 17, 20, 21],
  },
  {
    name: "16x4Odd",
    shuffle: [2, 3, 6, 7, 18, 19, 22, 23],
  },
  {
    name: "16x8Even_LowerHalf",
    shuffle: [0, 1, 4, 5, 8, 9, 12, 13],
  },
  {
    name: "16x8Odd_LowerHalf",
    shuffle: [2, 3, 6, 7, 10, 11, 14, 15],
  },
  {
    name: "16x8InterleaveLowHalves_LowerHalf",
    shuffle: [0, 1, 16, 17, 2, 3, 18, 19],
  },
  {
    name: "16x8InterleaveHighHalves_LowerHalf",
    shuffle: [8, 9, 24, 25, 10, 11, 26, 27],
  },
  {
    name: "16x4InterleaveHighHalves",
    shuffle: [4, 5, 20, 21, 6, 7, 22, 23],
  },
  {
    name: "16x8TransposeEven_LowerHalf",
    shuffle: [0, 1, 16, 17, 4, 5, 20, 21],
  },
  {
    name: "16x4TransposeOdd_LowerHalf",
    shuffle: [2, 3, 18, 19, 6, 7, 22, 23],
  },
  {
    name: "16x4ReverseBytes_LowerHalf",
    shuffle: [1, 0, 3, 2, 5, 4, 7, 6],
  },
  {
    name: "16x4Reverse_LowerHalf",
    shuffle: [6, 7, 4, 5, 2, 3, 0, 1],
  },
  {
    name: "16x2Reverse_LowerHalf",
    shuffle: [2, 3, 0, 1, 6, 7, 4, 5],
  },
  {
    name: "DupLow_Lane4_Input0",
    shuffle: [8, 9, 8, 9, 8, 9, 8, 9],
  },
  {
    name: "DupLow_Lane7_Input1",
    shuffle: [30, 31, 30, 31, 30, 31, 30, 31],
  }
];

(function Shuffle16x4TestAll() {
  const epilogue = [
    ...SimdInstr(kExprI32x4SConvertI16x8Low),
    kExprLocalTee, 0,
    kSimdPrefix, kExprI32x4ExtractLane, 0,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI32x4ExtractLane, 1,
    kExprI32Sub,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI32x4ExtractLane, 2,
    kExprI32Sub,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI32x4ExtractLane, 3,
    kExprI32Sub,
  ];
  for (const config of shuffle_16x4_tests) {
    ShuffleTest({
      ...config,
      epilogue: epilogue,
      sig: kSig_i_v,
    });
  }
})();

const shuffle_8x8_tests = [
  {
    name: "8x8Even",
    shuffle: [0, 2, 4, 6, 16, 18, 20, 22],
  },
  {
    name: "8x8Odd",
    shuffle: [1, 3, 5, 7, 17, 19, 21, 23],
  },
  {
    name: "8x16Even_LowerHalf",
    shuffle: [0, 2, 4, 6, 8, 10, 12, 14],
  },
  {
    name: "8x16Odd_LowerHalf",
    shuffle: [1, 3, 5, 7, 9, 11, 13, 15],
  },
  {
    name: "8x16InterleaveLowHalves_LowerHalf",
    shuffle: [0, 16, 1, 17, 2, 18, 3, 19],
  },
  {
    name: "8x16InterleaveHighHalves_LowerHalf",
    shuffle: [8, 24, 9, 25, 10, 26, 11, 27],
  },
  {
    name: "8x8InterleaveHighHalves",
    shuffle: [4, 20, 5, 21, 6, 22, 7, 23],
  },
  {
    name: "8x16TransposeEven_LowerHalf",
    shuffle: [0, 16, 2, 18, 4, 20, 6, 22],
  },
  {
    name: "8x16TransposeOdd_LowerHalf",
    shuffle: [1, 17, 3, 19, 5, 21, 7, 23],
  },
  {
    name: "DupLow_Lane4_Input0",
    shuffle: [4, 4, 4, 4, 4, 4, 4, 4],
  },
  {
    name: "DupLow_Lane12_Input1",
    shuffle: [28, 28, 28, 28, 28, 28, 28, 28],
  },
  {
    name: "8x8DeinterleaveEvenEven",
    shuffle: [0, 4, 8, 12, 16, 20, 24, 28],
  },
  {
    name: "8x8DeinterleaveOddEven",
    shuffle: [1, 5, 9, 13, 17, 21, 25, 29],
  },
  {
    name: "8x8DeinterleaveEvenOdd",
    shuffle: [2, 6, 10, 14, 18, 22, 26, 30],
  },
  {
    name: "8x8DeinterleaveOddOdd",
    shuffle: [3, 7, 11, 15, 19, 23, 27, 31],
  },
  {
    name: "16x4DeinterleaveEvenEven",
    shuffle: [0, 1, 8, 9, 16, 17, 24, 25],
  },
  {
    name: "16x4DeinterleaveOddEven",
    shuffle: [2, 3, 10, 11, 18, 19, 26, 27],
  },
  {
    name: "16x4DeinterleaveEvenOdd",
    shuffle: [4, 5, 12, 13, 20, 21, 28, 29],
  },
  {
    name: "16x4DeinterleaveOddOdd",
    shuffle: [6, 7, 14, 15, 22, 23, 30, 31],
  },
];

(function Shuffle8x8TestAll() {
  const epilogue = [
    ...SimdInstr(kExprI16x8SConvertI8x16Low),
    kExprLocalTee, 0,
    kSimdPrefix, kExprI16x8ExtractLaneU, 0,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI16x8ExtractLaneU, 1,
    kExprI32Add,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI16x8ExtractLaneU, 2,
    kExprI32Add,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI16x8ExtractLaneU, 3,
    kExprI32Add,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI16x8ExtractLaneU, 4,
    kExprI32Add,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI16x8ExtractLaneU, 5,
    kExprI32Add,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI16x8ExtractLaneU, 6,
    kExprI32Add,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI16x8ExtractLaneU, 7,
    kExprI32Add,
  ];
  for (const config of shuffle_8x8_tests) {
    ShuffleTest({
      ...config,
      epilogue: epilogue,
      sig: kSig_i_v,
    });
  }
})();
