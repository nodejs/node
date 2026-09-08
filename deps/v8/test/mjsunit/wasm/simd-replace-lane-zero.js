// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function ReplaceLaneZeroWithConstant() {
  const replace_lane_configs = [
    {
      name: 'i32x4 lane0',
      replace_opcode: kExprI32x4ReplaceLane,
      value_bytes: wasmI32Const(0x1234),
      view: (memory) => new Int32Array(memory.buffer, 0, 4),
      expected: [0x1234, 0, 0, 0],
    },
    {
      name: 'i64x2 lane0',
      replace_opcode: kExprI64x2ReplaceLane,
      value_bytes: wasmI64Const(0x1234),
      view: (memory) => new BigInt64Array(memory.buffer, 0, 2),
      expected: [0x1234n, 0n],
    },
    {
      name: 'f32x4 lane0',
      replace_opcode: kExprF32x4ReplaceLane,
      value_bytes: wasmF32Const(1.5),
      view: (memory) => new Float32Array(memory.buffer, 0, 4),
      expected: [1.5, 0, 0, 0],
    },
    {
      name: 'f64x2 lane0',
      replace_opcode: kExprF64x2ReplaceLane,
      value_bytes: wasmF64Const(2.25),
      view: (memory) => new Float64Array(memory.buffer, 0, 2),
      expected: [2.25, 0],
    },
  ];
  const kZeroVector = Array(16).fill(0);

  for (const config of replace_lane_configs) {
    print(config.name);
    const builder = new WasmModuleBuilder();
    builder.addMemory(1, 1, false);
    builder.exportMemoryAs('memory');

    builder.addFunction('replace_lane', kSig_v_v)
      .addBody([
        kExprI32Const, 0,
        kSimdPrefix, kExprS128Const, ...kZeroVector,
        ...config.value_bytes,
        kSimdPrefix, config.replace_opcode, 0,
        kSimdPrefix, kExprS128StoreMem, 0, 0,
      ])
      .exportFunc();

    const module = builder.instantiate();
    const memory = new Uint8Array(module.exports.memory.buffer);
    memory.fill(0x7f, 0, 16);

    module.exports.replace_lane();

    const view = config.view(memory);
    for (let i = 0; i < config.expected.length; ++i) {
      assertEquals(config.expected[i], view[i]);
    }
    // The rest should be zero.
    for (let i = view.byteLength; i < 16; ++i) {
      assertEquals(0, memory[i]);
    }
  }
})();

(function ReplaceLaneZero() {
  const replace_lane_local_configs = [
    {
      name: 'i32x4 lane0 local',
      replace_opcode: kExprI32x4ReplaceLane,
      sig: kSig_v_i,
      param_value: 0x5678,
      view: (memory) => new Int32Array(memory.buffer, 0, 4),
      expected: [0x5678, 0, 0, 0],
    },
    {
      name: 'i64x2 lane0 local',
      replace_opcode: kExprI64x2ReplaceLane,
      sig: kSig_v_l,
      param_value: 0x5678n,
      view: (memory) => new BigInt64Array(memory.buffer, 0, 2),
      expected: [0x5678n, 0n],
    },
    {
      name: 'f32x4 lane0 local',
      replace_opcode: kExprF32x4ReplaceLane,
      sig: kSig_v_f,
      param_value: 3.5,
      view: (memory) => new Float32Array(memory.buffer, 0, 4),
      expected: [3.5, 0, 0, 0],
    },
    {
      name: 'f64x2 lane0 local',
      replace_opcode: kExprF64x2ReplaceLane,
      sig: kSig_v_d,
      param_value: 4.75,
      view: (memory) => new Float64Array(memory.buffer, 0, 2),
      expected: [4.75, 0],
    },
  ];

  const kZeroVector = Array(16).fill(0);
  for (const config of replace_lane_local_configs) {
    print(config.name);
    const builder = new WasmModuleBuilder();
    builder.addMemory(1, 1, false);
    builder.exportMemoryAs('memory');

    builder.addFunction('replace_lane', config.sig)
      .addBody([
        kExprI32Const, 0,
        kSimdPrefix, kExprS128Const, ...kZeroVector,
        kExprLocalGet, 0,
        kSimdPrefix, config.replace_opcode, 0,
        kSimdPrefix, kExprS128StoreMem, 0, 0,
      ])
      .exportFunc();

    const module = builder.instantiate();
    const memory = new Uint8Array(module.exports.memory.buffer);
    memory.fill(0x7f, 0, 16);

    module.exports.replace_lane(config.param_value);

    const view = config.view(memory);
    for (let i = 0; i < config.expected.length; ++i) {
      assertEquals(config.expected[i], view[i]);
    }
    // The rest should be zero.
    for (let i = view.byteLength; i < 16; ++i) {
      assertEquals(0, memory[i]);
    }
  }
})();
