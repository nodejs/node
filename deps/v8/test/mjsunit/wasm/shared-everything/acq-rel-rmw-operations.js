// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-acquire-release

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestI32AtomicRmwAcqRel() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, true);
  builder.exportMemoryAs("memory");

  const kAcqRel = (kAtomicAcqRel << 4) | kAtomicAcqRel;

  builder.addFunction('i32_atomic_add_acq_rel', kSig_i_ii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,  // address and value
      kAtomicPrefix, kExprI32AtomicAdd, 0x22, kAcqRel, 0
    ])
    .exportFunc();

  builder.addFunction('i32_atomic_add8_u_acq_rel', kSig_i_ii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI32AtomicAdd8U, 0x20, kAcqRel, 0
    ])
    .exportFunc();

  builder.addFunction('i32_atomic_add16_u_acq_rel', kSig_i_ii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI32AtomicAdd16U, 0x21, kAcqRel, 0
    ])
    .exportFunc();

  builder.addFunction('i32_atomic_sub_acq_rel', kSig_i_ii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI32AtomicSub, 0x22, kAcqRel, 0
    ])
    .exportFunc();

  builder.addFunction('i32_atomic_and_acq_rel', kSig_i_ii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI32AtomicAnd, 0x22, kAcqRel, 0
    ])
    .exportFunc();

  builder.addFunction('i32_atomic_or_acq_rel', kSig_i_ii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI32AtomicOr, 0x22, kAcqRel, 0
    ])
    .exportFunc();

  builder.addFunction('i32_atomic_xor_acq_rel', kSig_i_ii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI32AtomicXor, 0x22, kAcqRel, 0
    ])
    .exportFunc();

  builder.addFunction('i32_atomic_xchg_acq_rel', kSig_i_ii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI32AtomicExchange, 0x22, kAcqRel, 0
    ])
    .exportFunc();

  builder.addFunction('i32_atomic_cmpxchg_acq_rel', kSig_i_iii)
    .addBody([
      kExprLocalGet, 0, // address
      kExprLocalGet, 1, // expected
      kExprLocalGet, 2, // new_value
      kAtomicPrefix, kExprI32AtomicCompareExchange, 0x22, kAcqRel, 0
    ])
    .exportFunc();

  let instance = builder.instantiate();
  let exports = instance.exports;
  let i32 = new Int32Array(instance.exports.memory.buffer);
  let u8 = new Uint8Array(instance.exports.memory.buffer);
  let u16 = new Uint16Array(instance.exports.memory.buffer);

  i32[0] = 10;
  assertEquals(10, exports.i32_atomic_add_acq_rel(0, 5));
  assertEquals(15, i32[0]);

  u8[0] = 10;
  assertEquals(10, exports.i32_atomic_add8_u_acq_rel(0, 5));
  assertEquals(15, u8[0]);

  u16[0] = 10;
  assertEquals(10, exports.i32_atomic_add16_u_acq_rel(0, 5));
  assertEquals(15, u16[0]);

  i32[0] = 15;
  assertEquals(15, exports.i32_atomic_sub_acq_rel(0, 7));
  assertEquals(8, i32[0]);

  i32[0] = 0b1100;
  assertEquals(0b1100, exports.i32_atomic_and_acq_rel(0, 0b1010));
  assertEquals(0b1000, i32[0]);

  i32[0] = 0b1000;
  assertEquals(0b1000, exports.i32_atomic_or_acq_rel(0, 0b0101));
  assertEquals(0b1101, i32[0]);

  i32[0] = 0b1101;
  assertEquals(0b1101, exports.i32_atomic_xor_acq_rel(0, 0b0110));
  assertEquals(0b1011, i32[0]);

  i32[0] = 0b1011;
  assertEquals(0b1011, exports.i32_atomic_xchg_acq_rel(0, 12345));
  assertEquals(12345, i32[0]);

  i32[0] = 10;
  assertEquals(10, exports.i32_atomic_cmpxchg_acq_rel(0, 10, 20));
  assertEquals(20, i32[0]);
  assertEquals(20, exports.i32_atomic_cmpxchg_acq_rel(0, 10, 30));
  assertEquals(20, i32[0]);
})();

(function TestI64AtomicRmwAcqRel() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, true);
  builder.exportMemoryAs("memory");

  const kSig_l_il = makeSig([kWasmI32, kWasmI64], [kWasmI64]);
  const kSig_l_ill = makeSig([kWasmI32, kWasmI64, kWasmI64], [kWasmI64]);

  const kAcqRel = (kAtomicAcqRel << 4) | kAtomicAcqRel;

  builder.addFunction('i64_atomic_add_acq_rel', kSig_l_il)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI64AtomicAdd, 0x23, kAcqRel, 0
    ]).exportFunc();

  builder.addFunction('i64_atomic_add8_u_acq_rel', kSig_l_il)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI64AtomicAdd8U, 0x20, kAcqRel, 0
    ]).exportFunc();

  builder.addFunction('i64_atomic_add16_u_acq_rel', kSig_l_il)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI64AtomicAdd16U, 0x21, kAcqRel, 0
    ]).exportFunc();

  builder.addFunction('i64_atomic_add32_u_acq_rel', kSig_l_il)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI64AtomicAdd32U, 0x22, kAcqRel, 0
    ]).exportFunc();

  builder.addFunction('i64_atomic_sub_acq_rel', kSig_l_il)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI64AtomicSub, 0x23, kAcqRel, 0
    ]).exportFunc();

  builder.addFunction('i64_atomic_and_acq_rel', kSig_l_il)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI64AtomicAnd, 0x23, kAcqRel, 0
    ]).exportFunc();

  builder.addFunction('i64_atomic_or_acq_rel', kSig_l_il)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI64AtomicOr, 0x23, kAcqRel, 0
    ]).exportFunc();

  builder.addFunction('i64_atomic_xor_acq_rel', kSig_l_il)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI64AtomicXor, 0x23, kAcqRel, 0
    ]).exportFunc();

  builder.addFunction('i64_atomic_xchg_acq_rel', kSig_l_il)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI64AtomicExchange, 0x23, kAcqRel, 0
    ]).exportFunc();

  builder.addFunction('i64_atomic_cmpxchg_acq_rel', kSig_l_ill)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2,
      kAtomicPrefix, kExprI64AtomicCompareExchange, 0x23, kAcqRel, 0
    ]).exportFunc();

  let instance = builder.instantiate();
  let exports = instance.exports;
  let i64 = new BigInt64Array(instance.exports.memory.buffer);
  let u8 = new Uint8Array(instance.exports.memory.buffer);
  let u16 = new Uint16Array(instance.exports.memory.buffer);
  let u32 = new Uint32Array(instance.exports.memory.buffer);

  i64[0] = 10n;
  assertEquals(10n, exports.i64_atomic_add_acq_rel(0, 5n));
  assertEquals(15n, i64[0]);

  u8[0] = 10;
  assertEquals(10n, exports.i64_atomic_add8_u_acq_rel(0, 5n));
  assertEquals(15, u8[0]);

  u16[0] = 10;
  assertEquals(10n, exports.i64_atomic_add16_u_acq_rel(0, 5n));
  assertEquals(15, u16[0]);

  u32[0] = 10;
  assertEquals(10n, exports.i64_atomic_add32_u_acq_rel(0, 5n));
  assertEquals(15, u32[0]);

  i64[0] = 15n;
  assertEquals(15n, exports.i64_atomic_sub_acq_rel(0, 7n));
  assertEquals(8n, i64[0]);

  i64[0] = 0b1100n;
  assertEquals(0b1100n, exports.i64_atomic_and_acq_rel(0, 0b1010n));
  assertEquals(0b1000n, i64[0]);

  i64[0] = 0b1000n;
  assertEquals(0b1000n, exports.i64_atomic_or_acq_rel(0, 0b0101n));
  assertEquals(0b1101n, i64[0]);

  i64[0] = 0b1101n;
  assertEquals(0b1101n, exports.i64_atomic_xor_acq_rel(0, 0b0110n));
  assertEquals(0b1011n, i64[0]);

  i64[0] = 0b1011n;
  assertEquals(0b1011n, exports.i64_atomic_xchg_acq_rel(0, 1234567890n));
  assertEquals(1234567890n, i64[0]);

  i64[0] = 10n;
  assertEquals(10n, exports.i64_atomic_cmpxchg_acq_rel(0, 10n, 20n));
  assertEquals(20n, i64[0]);
  assertEquals(20n, exports.i64_atomic_cmpxchg_acq_rel(0, 10n, 30n));
  assertEquals(20n, i64[0]);
})();
