// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// 32769 pages * 64 KiB = 0x8001_0000 bytes (just over 2 GiB).
// Shared memory32 reserves the full max, so this is a virtual reservation.
const kPages = 32769;
const memory = new WebAssembly.Memory(
    {initial: kPages, maximum: kPages, shared: true});

const builder = new WasmModuleBuilder();
builder.addImportedMemory('m', 'memory', kPages, kPages, 'shared');

builder.addFunction('probe',
    makeSig([kWasmI32, kWasmI32], [kWasmI32]))
  .addBody([
    kExprLocalGet, 0,      // index (i32)
    kExprLocalGet, 1,      // expected (i32)
    kExprI64Const, 0,      // timeout = 0 ns -> never blocks
    kAtomicPrefix, kExprI32AtomicWait, /*align=*/2, /*offset=*/0,
  ])
  .exportFunc();

builder.addFunction('probe_offset',
    makeSig([kWasmI32, kWasmI32], [kWasmI32]))
  .addBody([
    kExprLocalGet, 0,      // index (i32)
    kExprLocalGet, 1,      // expected (i32)
    kExprI64Const, 0,      // timeout = 0 ns -> never blocks
    kAtomicPrefix, kExprI32AtomicWait, /*align=*/2, /*offset=*/4,
  ])
  .exportFunc();

builder.addFunction('probe64',
    makeSig([kWasmI32, kWasmI64], [kWasmI32]))
  .addBody([
    kExprLocalGet, 0,      // index (i32)
    kExprLocalGet, 1,      // expected (i64)
    kExprI64Const, 0,      // timeout = 0 ns -> never blocks
    kAtomicPrefix, kExprI64AtomicWait, /*align=*/3, /*offset=*/0,
  ])
  .exportFunc();

builder.addFunction('probe64_offset',
    makeSig([kWasmI32, kWasmI64], [kWasmI32]))
  .addBody([
    kExprLocalGet, 0,      // index (i32)
    kExprLocalGet, 1,      // expected (i64)
    kExprI64Const, 0,      // timeout = 0 ns -> never blocks
    kAtomicPrefix, kExprI64AtomicWait, /*align=*/3, /*offset=*/8,
  ])
  .exportFunc();

const instance = builder.instantiate({m: {memory: memory}});

// Test boundary index values:
// 1. Minimum index 0 (within Smi range).
assertEquals(kAtomicWaitTimedOut, instance.exports.probe(0, 0));
assertEquals(kAtomicWaitTimedOut, instance.exports.probe64(0, 0n));

// 2. Crossing the 2 GiB boundary via non-zero immediate offset:
// 0x7ffffffc + 4 = 0x80000000
assertEquals(kAtomicWaitTimedOut, instance.exports.probe_offset(0x7ffffffc | 0, 0));
assertEquals(kAtomicWaitTimedOut, instance.exports.probe64_offset(0x7ffffff8 | 0, 0n));

// 3. Index at >= 2 GiB (0x80000000):
// On 64-bit architectures, Liftoff must zero-extend the 32-bit index when
// passing it as pointer-sized offset to the AtomicWait builtin, rather than
// sign-extending it to 0xFFFF_FFFF_8000_0000.
assertEquals(kAtomicWaitTimedOut, instance.exports.probe(0x80000000 | 0, 0));
assertEquals(kAtomicWaitTimedOut, instance.exports.probe64(0x80000000 | 0, 0n));
assertEquals(kAtomicWaitTimedOut, instance.exports.probe_offset(0x80000000 | 0, 0));
assertEquals(kAtomicWaitTimedOut, instance.exports.probe64_offset(0x80000000 | 0, 0n));

// 4. Out-of-bounds index (0x80010000 >= memory size):
// Assert that bounds checking remains intact after zero-extension and pointer
// math refactoring.
assertTraps(kTrapMemOutOfBounds, () => instance.exports.probe(0x80010000 | 0, 0));
assertTraps(kTrapMemOutOfBounds, () => instance.exports.probe64(0x80010000 | 0, 0n));
assertTraps(kTrapMemOutOfBounds, () => instance.exports.probe_offset(0x80010000 | 0, 0));
assertTraps(kTrapMemOutOfBounds, () => instance.exports.probe64_offset(0x80010000 | 0, 0n));
