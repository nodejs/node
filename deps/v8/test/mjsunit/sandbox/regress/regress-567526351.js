// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const mem1 = new WebAssembly.Memory(
    {initial: 1n, maximum: 10n, shared: true, address: 'i64'});
const mem2 = new WebAssembly.Memory(
    {initial: 1n, maximum: 10n, shared: true, address: 'i64'});

const builder = new WasmModuleBuilder();
builder.addImportedMemory('m', 'mem', 1, 10, true, true);
builder.addFunction('size', makeSig([], [kWasmI64]))
    .addBody([kExprMemorySize, 0])
    .exportFunc();
builder.addFunction('fill', makeSig([kWasmI64, kWasmI32, kWasmI64], []))
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kNumericPrefix, kExprMemoryFill, 0,
    ])
    .exportFunc();

const instance = builder.instantiate({m: {mem: mem1}});

// Overwrite mem1's managed_backing_store with mem2's so that GC collects
// mem1's CppGCManaged<BackingStore> while the instance remains alive.
const kManagedBackingStoreOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('WASM_MEMORY_OBJECT_TYPE'),
    'managed_backing_store');
Sandbox.corruptObjectField(
    mem1, kManagedBackingStoreOffset,
    Sandbox.readObjectField(mem2, kManagedBackingStoreOffset, 32), 32);

gc();
gc();

assertThrows(
    () => instance.exports.fill(0x20000000000n, 0x42, 8n),
    WebAssembly.RuntimeError);
assertEquals(1n, instance.exports.size());

// Growing mem2 triggers UpdateSharedWasmMemoryObjects, which must not
// re-read mem1's corrupted managed_backing_store into the instance.
mem2.grow(1n);
assertEquals(1n, instance.exports.size());
