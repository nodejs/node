// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const $sig = builder.addType(kSig_v_v);
const $f0 = builder.addFunction('f0', $sig).addBody([]);

builder.addTable(wasmRefType($sig), 1, 1, [kExprRefFunc, $f0.index])
    .exportAs('table0');
builder.addTable(wasmRefNullType($sig), 1, 1)
    .exportAs('table1');

const instance = builder.instantiate();
const {table0, table1} = instance.exports;

const kHeapObjectTag = 1;
const kWasmTableType = Sandbox.getInstanceTypeIdFor('WASM_TABLE_OBJECT_TYPE');
const kWasmTableRawTypeOffset =
    Sandbox.getFieldOffset(kWasmTableType, 'raw_type');
const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

function getPtr(obj) {
  return Sandbox.getAddressOf(obj) + kHeapObjectTag;
}
function getField(obj, offset) {
  return memory.getUint32(obj + offset - kHeapObjectTag, true);
}
function setField(obj, offset, value) {
  memory.setUint32(obj + offset - kHeapObjectTag, value, true);
}

// Corrupt non-nullable table0's in-sandbox raw_type to be nullable.
const nullable_raw_type = getField(getPtr(table1), kWasmTableRawTypeOffset);
setField(getPtr(table0), kWasmTableRawTypeOffset, nullable_raw_type);

// Clearing an entry in a non-nullable WasmDispatchTable must fail an SBXCHECK.
table0.set(0, null);

assertUnreachable('Process should have been killed.');
