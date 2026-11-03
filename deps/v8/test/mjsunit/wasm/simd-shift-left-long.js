// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/value-helper.js');

const shift_left_long_configs = [
  {
    name: 'i8x16 -> i16x8',
    source_bits: 8,
    values: int8_array,
    sig: kSig_i_i,
    splat_opcode: kExprI8x16Splat,
    convert_opcodes: [kExprI16x8SConvertI8x16Low, kExprI16x8SConvertI8x16High],
    shift_opcode: kExprI16x8Shl,
    extract_opcode: kExprI16x8ExtractLaneS,
  },
  {
    name: 'u8x16 -> u16x8',
    source_bits: 8,
    values: new Uint8Array(int8_array),
    sig: kSig_i_i,
    splat_opcode: kExprI8x16Splat,
    convert_opcodes: [kExprI16x8UConvertI8x16Low, kExprI16x8UConvertI8x16High],
    shift_opcode: kExprI16x8Shl,
    extract_opcode: kExprI16x8ExtractLaneS,
  },
  {
    name: 'i16x8 -> i32x4',
    source_bits: 16,
    values: int16_array,
    sig: kSig_i_i,
    splat_opcode: kExprI16x8Splat,
    convert_opcodes: [kExprI32x4SConvertI16x8Low, kExprI32x4SConvertI16x8High],
    shift_opcode: kExprI32x4Shl,
    extract_opcode: kExprI32x4ExtractLane,
  },
  {
    name: 'u16x8 -> u32x4',
    source_bits: 16,
    values: new Uint16Array(int16_array),
    sig: kSig_i_i,
    splat_opcode: kExprI16x8Splat,
    convert_opcodes: [kExprI32x4UConvertI16x8Low, kExprI32x4UConvertI16x8High],
    shift_opcode: kExprI32x4Shl,
    extract_opcode: kExprI32x4ExtractLane,
  },
  {
    name: 'i32x4 -> i64x2',
    source_bits: 32,
    values: int32_array,
    sig: kSig_l_i,
    splat_opcode: kExprI32x4Splat,
    convert_opcodes: [kExprI64x2SConvertI32x4Low, kExprI64x2SConvertI32x4High],
    shift_opcode: kExprI64x2Shl,
    extract_opcode: kExprI64x2ExtractLane,
  },
  {
    name: 'u32x4 -> u64x2',
    source_bits: 32,
    values: new Uint32Array(int32_array),
    sig: kSig_l_i,
    splat_opcode: kExprI32x4Splat,
    convert_opcodes: [kExprI64x2UConvertI32x4Low, kExprI64x2UConvertI32x4High],
    shift_opcode: kExprI64x2Shl,
    extract_opcode: kExprI64x2ExtractLane,
  },
];

function TestShiftLeftLong(config, high) {
  const source_bits = config.source_bits;
  const lane_bits = 2 * source_bits;
  for (let shift = -lane_bits; shift <= lane_bits + source_bits; ++shift) {
    const builder = new WasmModuleBuilder();
    builder.addFunction('shift_left_long', config.sig)
        .addBody([
          kExprLocalGet, 0,
          ...SimdInstr(config.splat_opcode),
          ...SimdInstr(config.convert_opcodes[high]),
          ...wasmI32Const(shift),
          ...SimdInstr(config.shift_opcode),
          ...SimdInstr(config.extract_opcode), 0,
        ])
        .exportFunc();

    const wasm = builder.instantiate().exports;
    const shift_amount = BigInt(shift & (lane_bits - 1));
    for (const value of config.values) {
      const result = wasm.shift_left_long(value);
      const expected = BigInt.asIntN(lane_bits, BigInt(value) << shift_amount);
      assertEquals(lane_bits === 64 ? expected : Number(expected), result);
    }
  }
}

for (const config of shift_left_long_configs) {
  TestShiftLeftLong(config, 0);
  TestShiftLeftLong(config, 1);
}
