// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-fp16 --no-liftoff --sim-arm64-optional-features=all

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const vectorA = [
  0x00, 0x3c, // 1.0
  0x00, 0x40, // 2.0
  0x00, 0x42, // 3.0
  0x00, 0x44, // 4.0
  0x00, 0x45, // 5.0
  0x00, 0x46, // 6.0
  0x00, 0x47, // 7.0
  0x00, 0x48, // 8.0
];

const expected = [
  0x4000, // 1.0 * 2.0 = 2.0
  0x4400, // 2.0 * 2.0 = 4.0
  0x4600, // 3.0 * 2.0 = 6.0
  0x4800, // 4.0 * 2.0 = 8.0
  0x4900, // 5.0 * 2.0 = 10.0
  0x4a00, // 6.0 * 2.0 = 12.0
  0x4b00, // 7.0 * 2.0 = 14.0
  0x4c00, // 8.0 * 2.0 = 16.0
];

for (let lane = 0; lane < 8; lane++) {
  for (const splat_on_right of [true, false]) {
    const builder = new WasmModuleBuilder();
    builder.addMemory(1, 1);
    builder.exportMemoryAs('memory');

    const vectorB = new Array(16).fill(0);
    vectorB[lane * 2] = 0x00;
    vectorB[lane * 2 + 1] = 0x40; // 2.0 in FP16

    const b0 = lane * 2;
    const b1 = lane * 2 + 1;
    const shuffle = [];
    for (let i = 0; i < 8; i++) {
      shuffle.push(b0, b1);
    }

    const opA = [
      ...wasmS128Const(vectorA),
    ];
    const opB = [
      ...wasmS128Const(vectorB),
      ...wasmS128Const(vectorB),
      kSimdPrefix, kExprI8x16Shuffle,
      ...shuffle,
    ];

    const lhs = splat_on_right ? opA : opB;
    const rhs = splat_on_right ? opB : opA;

    builder.addFunction('test', kSig_v_v)
      .addBody([
        kExprI32Const, 0,
        ...lhs,
        ...rhs,
        kSimdPrefix, ...wasmSignedLeb(0x13f), // f16x8.mul
        kSimdPrefix, kExprS128StoreMem, 0, 0,
      ])
      .exportFunc();

    const instance = builder.instantiate();
    instance.exports.test();

    const memoryView = new DataView(instance.exports.memory.buffer);
    for (let i = 0; i < 8; i++) {
      assertEquals(expected[i], memoryView.getUint16(i * 2, true),
                   `Mismatch at lane ${i} (splat lane ${lane}, splat_on_right: ${splat_on_right})`);
    }
  }
}
