// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-acquire-release

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
builder.addMemory(1, 1, true);

// Atomic loads (acquire)
builder.addFunction('i32_atomic_load_acquire', kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kAtomicPrefix, kExprI32AtomicLoad, 0x22, kAtomicAcqRel, 0
    ])
    .exportFunc();

builder.addFunction('i32_atomic_load8_u_acquire', kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kAtomicPrefix, kExprI32AtomicLoad8U, 0x20, kAtomicAcqRel, 0
    ])
    .exportFunc();

builder.addFunction('i32_atomic_load16_u_acquire', kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kAtomicPrefix, kExprI32AtomicLoad16U, 0x21, kAtomicAcqRel, 0
    ])
    .exportFunc();

builder.addFunction('i64_atomic_load_acquire', kSig_l_i)
  .addBody([
    kExprLocalGet, 0,
    kAtomicPrefix, kExprI64AtomicLoad, 0x23, kAtomicAcqRel, 0
  ])
  .exportFunc();

builder.addFunction('i64_atomic_load8_u_acquire', kSig_l_i)
    .addBody([
      kExprLocalGet, 0,
      kAtomicPrefix, kExprI64AtomicLoad8U, 0x20, kAtomicAcqRel, 0
    ])
    .exportFunc();

builder.addFunction('i64_atomic_load16_u_acquire', kSig_l_i)
    .addBody([
      kExprLocalGet, 0,
      kAtomicPrefix, kExprI64AtomicLoad16U, 0x21, kAtomicAcqRel, 0
    ])
    .exportFunc();

builder.addFunction('i64_atomic_load32_u_acquire', kSig_l_i)
    .addBody([
      kExprLocalGet, 0,
      kAtomicPrefix, kExprI64AtomicLoad32U, 0x22, kAtomicAcqRel, 0
    ])
    .exportFunc();


// Atomic stores (release)
builder.addFunction('i32_atomic_store_release', kSig_i_ii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI32AtomicStore, 0x22, kAtomicAcqRel, 0,
      kExprLocalGet, 0,
      kAtomicPrefix, kExprI32AtomicLoad, 2, 0,
    ])
    .exportFunc();

builder.addFunction('i32_atomic_store8_u_release', kSig_i_ii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI32AtomicStore8U, 0x20, kAtomicAcqRel, 0,
      kExprLocalGet, 0,
      kAtomicPrefix, kExprI32AtomicLoad8U, 0, 0,
    ])
    .exportFunc();

builder.addFunction('i32_atomic_store16_u_release', kSig_i_ii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI32AtomicStore16U, 0x21, kAtomicAcqRel, 0,
      kExprLocalGet, 0,
      kAtomicPrefix, kExprI32AtomicLoad16U, 1, 0,
    ])
    .exportFunc();

builder.addFunction(
    'i64_atomic_store_release', makeSig([kWasmI32, kWasmI64], [kWasmI64]))
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI64AtomicStore, 0x23, kAtomicAcqRel, 0,
      kExprLocalGet, 0,
      kAtomicPrefix, kExprI64AtomicLoad, 3, 0,
    ])
    .exportFunc();

builder.addFunction(
    'i64_atomic_store8_u_release',
    makeSig([kWasmI32, kWasmI64], [kWasmI64]))
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI64AtomicStore8U, 0x20, kAtomicAcqRel, 0,
      kExprLocalGet, 0,
      kAtomicPrefix, kExprI64AtomicLoad8U, 0, 0,
    ])
    .exportFunc();

builder.addFunction(
    'i64_atomic_store16_u_release',
    makeSig([kWasmI32, kWasmI64], [kWasmI64]))
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI64AtomicStore16U, 0x21, kAtomicAcqRel, 0,
      kExprLocalGet, 0, kAtomicPrefix, kExprI64AtomicLoad16U, 1, 0,
    ])
    .exportFunc();

builder.addFunction(
    'i64_atomic_store32_u_release',
    makeSig([kWasmI32, kWasmI64], [kWasmI64]))
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kAtomicPrefix, kExprI64AtomicStore32U, 0x22, kAtomicAcqRel, 0,
      kExprLocalGet, 0, kAtomicPrefix, kExprI64AtomicLoad32U, 2, 0,
    ])
    .exportFunc();

let instance = builder.instantiate();
let exports = instance.exports;

exports.i64_atomic_store_release(0, 0x1234567890abcdefn)

assertEquals(0x90abcdef | 0, exports.i32_atomic_load_acquire(0));
assertEquals(0xef, exports.i32_atomic_load8_u_acquire(0));
assertEquals(0xcdef, exports.i32_atomic_load16_u_acquire(0));
assertEquals(0x1234567890abcdefn, exports.i64_atomic_load_acquire(0));
assertEquals(0xefn, exports.i64_atomic_load8_u_acquire(0));
assertEquals(0xcdefn, exports.i64_atomic_load16_u_acquire(0));
assertEquals(0x90abcdefn, exports.i64_atomic_load32_u_acquire(0));

assertEquals(42, exports.i32_atomic_store_release(0, 42));
assertEquals(0xab, exports.i32_atomic_store8_u_release(0, 0xab));
assertEquals(0xabcd, exports.i32_atomic_store16_u_release(2, 0xabcd));
assertEquals(42n, exports.i64_atomic_store_release(8, 42n));
assertEquals(0xabn, exports.i64_atomic_store8_u_release(0, 0xabn));
assertEquals(0xabcdn, exports.i64_atomic_store16_u_release(2, 0xabcdn));
assertEquals(
    0x1234abcdn, exports.i64_atomic_store32_u_release(4, 0x1234abcdn));
