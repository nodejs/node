// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function Test(config) {
  print(config.name);
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.exportMemoryAs('memory');
  builder.addFunction('shuffle', makeSig([kWasmI32, kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 2,
        kExprLocalGet, 0,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprLocalGet, 1,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kSimdPrefix, kExprI8x16Shuffle,
        ...config.shuffle,
        ...config.dup_shuffle,
        kSimdPrefix, kExprS128StoreMem, 0, 0,
        ])
      .exportFunc();

  const module = builder.instantiate();
  // Initialize memory:
  // 2x16-bytes for inputs.
  const result_array = new Uint8Array(module.exports.memory.buffer);
  for (let i = 0; i < 32; ++i) {
    result_array[i] = i;
  }

  const expected_results = new Uint8Array(16);
  // Low half of the result.
  for (let i = 0; i < 8; ++i) {
    const idx = config.shuffle[i];
    expected_results[i] = result_array[idx];
  }
  // Top half.
  for (let i = 0; i < 8; ++i) {
    const idx = config.dup_shuffle[i];
    expected_results[i+8] = result_array[idx];
  }

  const src0 = 0;
  const src1 = 16;
  const dst = 32;
  module.exports.shuffle(src0, src1, dst);
  for (let i = 0; i < 16; ++i) {
    assertEquals(expected_results[i], result_array[dst + i]);
  }
}

(function SplatAndShuffleTest(config) {
  const dup_byte_0 = [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 ]
  const dup_half_8 = [0x10, 0x11, 0x10, 0x11, 0x10, 0x11, 0x10, 0x11 ]
  const dup_word_1 = [0x04, 0x05, 0x06, 0x07, 0x04, 0x05, 0x06, 0x07 ]

  const even_bytes = [0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e ]
  const interleave_high_halves = [0x08, 0x09, 0x18, 0x19, 0x0a, 0x0b, 0x1a, 0x1b ]
  const transpose_odd_words = [0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17 ]

  const splat_and_shuffle_tests =[
    {
      name: "dup and even",
      dup_shuffle: dup_byte_0,
      shuffle: even_bytes,
    },
    {
      name: "dup and interleave high",
      dup_shuffle: dup_half_8,
      shuffle: interleave_high_halves,
    },
    {
      name: "dup and transpose odd",
      dup_shuffle: dup_word_1,
      shuffle: transpose_odd_words,
    },
  ];

  for (const config of splat_and_shuffle_tests) {
    Test(config);
  }
})();
