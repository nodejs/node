// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const getField = (obj, offset) => memory.getUint32(obj + offset, true);
const setField = (obj, offset, value) =>
    memory.setUint32(obj + offset, value, true);

const kTrustedPointerHandleShift =
    Sandbox.getMetadata().trustedPointerHandleShift;
const kTrustedPointerHandleStride = 1 << kTrustedPointerHandleShift;

// When a table has a fixed size (initial_size == maximum_size), compiled
// call_indirect checks the index against the static kFixedTableSize constant
// rather than loading the dynamic length of dispatch_table0 (which is 0 on a
// partially-initialized WasmTrustedInstanceData). Any index in
// [0, kFixedTableSize) is therefore out-of-bounds for the 0-length
// empty_dispatch_table on a failed instance.
const kFixedTableSize = 16;
const kOutOfBoundsTableIndex = 0;

const builder = new WasmModuleBuilder();
const sig = builder.addType(kSig_i_v);
builder.addImportedMemory('m', 'memory', 1, 1);
const table0 =
    builder.addTable(wasmRefNullType(sig), kFixedTableSize, kFixedTableSize);
const table1 = builder.addTable(kWasmFuncRef, 1, 1).exportAs('table');
builder.addFunction('caller', sig).addBody([
  kExprI32Const, kOutOfBoundsTableIndex,
  kExprCallIndirect, sig, table0.index,
]);
builder.addActiveElementSegment(table1.index, wasmI32Const(0), [0]);
const module = builder.toModule();

const wasmMemory = new WebAssembly.Memory({initial: 1, maximum: 1});
const good = new WebAssembly.Instance(module, {m: {memory: wasmMemory}});
const goodAddr = Sandbox.getAddressOf(good);
const instanceTypeId = Sandbox.getInstanceTypeIdOf(good);
const trustedDataOffset =
    Sandbox.getFieldOffset(instanceTypeId, 'trusted_data');
const goodHandle = getField(goodAddr, trustedDataOffset);

// Trigger failed instantiation due to an invalid memory import.
assertThrows(
    () => new WebAssembly.Instance(module, {m: {memory: 1}}),
    WebAssembly.LinkError);

const after = new WebAssembly.Instance(module, {m: {memory: wasmMemory}});
const afterHandle = getField(Sandbox.getAddressOf(after), trustedDataOffset);

// Attempt to resolve the failed instance's WasmTrustedInstanceData handle
// (allocated between goodHandle and afterHandle) through the lazy table entry.
for (let handle = goodHandle + kTrustedPointerHandleStride;
     handle < afterHandle; handle += kTrustedPointerHandleStride) {
  setField(goodAddr, trustedDataOffset, handle);
  good.exports.table.get(0)();
}
