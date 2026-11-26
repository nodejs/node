// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless --sandbox-testing

// Regression test: array.new_data validates the array element layout against
// the trusted array type. Expected to abort via SBXCHECK.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Same corruption technique/offsets as
// sandbox/drumbrake/array-element-size-confusion.js.
const kHeapObjectTag = 1;
const kMapOffset = 0;
const kMapTypeInfoOffset = 0x14;
const kCanonicalElementTypeOffset = 0x8;

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
function getPtr(obj) {
  return Sandbox.getAddressOf(obj) + kHeapObjectTag;
}
function getField(obj, offset) {
  return memory.getUint32(obj + offset - kHeapObjectTag, true);
}
function setField(obj, offset, value) {
  memory.setUint32(obj + offset - kHeapObjectTag, value, true);
}

const builder = new WasmModuleBuilder();
const $u8arr = builder.addArray(kWasmI8, {mutability: true});
const $s128arr = builder.addArray(kWasmS128, {mutability: true});
const data = builder.addPassiveDataSegment(new Array(64).fill(0));

builder.addFunction('alloc_u8', makeSig([kWasmI32], [wasmRefType($u8arr)]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprArrayNewDefault, $u8arr])
    .exportFunc();
builder.addFunction('alloc_s128', makeSig([kWasmI32], [wasmRefType($s128arr)]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprArrayNewDefault, $s128arr])
    .exportFunc();
builder
    .addFunction(
        'new_data_u8', makeSig([kWasmI32, kWasmI32], [wasmRefType($u8arr)]))
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1, kGCPrefix, kExprArrayNewData,
      $u8arr, data
    ])
    .exportFunc();

const instance = builder.instantiate();
const {alloc_u8, alloc_s128, new_data_u8} = instance.exports;

const u8 = alloc_u8(1);
const s128 = alloc_s128(1);

// Control: array.new_data on a genuine i8 array type works fine.
new_data_u8(0, 64);
print('RESULT: control new_data ok');

// Learn the raw canonical_element_type_ bit pattern used for a real s128
// array type directly from its own WasmTypeInfo.
const s128_map = getField(getPtr(s128), kMapOffset);
const s128_type_info = getField(s128_map, kMapTypeInfoOffset);
const s128_raw_element_type =
    getField(s128_type_info, kCanonicalElementTypeOffset);

// Corrupt the (shared, canonical) WasmTypeInfo for the i8 array *type* so
// that element_type() reports s128 elements, while the type's Map still
// encodes the true i8 stride.
const u8_map = getField(getPtr(u8), kMapOffset);
const u8_type_info = getField(u8_map, kMapTypeInfoOffset);
setField(u8_type_info, kCanonicalElementTypeOffset, s128_raw_element_type);

print(
    'RESULT: about to array.new_data with corrupted element type -> ' +
    'expect crash');
new_data_u8(0, 64);
assertUnreachable('Process should have been killed.');
