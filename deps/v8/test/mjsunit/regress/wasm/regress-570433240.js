// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-fp16

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1, false);
builder.exportMemoryAs("memory");

builder.addFunction("testNeg", makeSig([kWasmI32, kWasmI32], []))
  .addBody([
    kExprLocalGet, 1, // dst
    kExprLocalGet, 0, // src
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kSimdPrefix, ...kExprF16x8Neg,
    kSimdPrefix, kExprS128StoreMem, 0, 0,
  ])
  .exportFunc();

builder.addFunction("testAbs", makeSig([kWasmI32, kWasmI32], []))
  .addBody([
    kExprLocalGet, 1, // dst
    kExprLocalGet, 0, // src
    kSimdPrefix, kExprS128LoadMem, 0, 0,
    kSimdPrefix, ...kExprF16x8Abs,
    kSimdPrefix, kExprS128StoreMem, 0, 0,
  ])
  .exportFunc();

const instance = builder.instantiate();
const dv = new DataView(instance.exports.memory.buffer);

const testValues = [
  0x7e01, // positive quiet NaN with payload 1
  0xfe01, // negative quiet NaN with payload 1
  0x7e55, // positive quiet NaN with payload 0x55
  0xfeaa, // negative quiet NaN with payload 0x2aa
  0x7c01, // positive signaling NaN with payload 1
  0xfc01, // negative signaling NaN with payload 1
  0x0000, // +0.0
  0x8000, // -0.0
  0x7c00, // +infinity
  0xfc00, // -infinity
  0x3c00, // +1.0
  0xbc00, // -1.0
  0x0001, // subnormal
  0x8001, // -subnormal
  0x7fff, // quiet NaN all payload bits 1
  0xffff, // -quiet NaN all payload bits 1
];

// Test batches of 8 values
for (let offset = 0; offset + 8 <= testValues.length; offset += 8) {
  for (let i = 0; i < 8; i++) {
    dv.setUint16(i * 2, testValues[offset + i], true);
  }

  instance.exports.testNeg(0, 16);
  for (let i = 0; i < 8; i++) {
    const expected = testValues[offset + i] ^ 0x8000;
    assertEquals(expected, dv.getUint16((8 + i) * 2, true), `f16x8.neg mismatch at index ${offset + i}`);
  }

  instance.exports.testAbs(0, 16);
  for (let i = 0; i < 8; i++) {
    const expected = testValues[offset + i] & 0x7fff;
    assertEquals(expected, dv.getUint16((8 + i) * 2, true), `f16x8.abs mismatch at index ${offset + i}`);
  }
}
