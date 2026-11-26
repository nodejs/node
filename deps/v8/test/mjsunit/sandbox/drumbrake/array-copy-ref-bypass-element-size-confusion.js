// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless --sandbox-testing

// Regression test: array.copy validates the element layout of both arrays
// against their trusted array types. Expected to abort via SBXCHECK.

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
const $refarr = builder.addArray(kWasmExternRef, {mutability: true});

builder.addFunction('alloc_u8', makeSig([kWasmI32], [wasmRefType($u8arr)]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprArrayNewDefault, $u8arr])
    .exportFunc();
builder.addFunction('alloc_ref', makeSig([kWasmI32], [wasmRefType($refarr)]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprArrayNewDefault, $refarr])
    .exportFunc();
builder
    .addFunction(
        'copy_u8',
        makeSig(
            [
              wasmRefType($u8arr), kWasmI32, wasmRefType($u8arr), kWasmI32,
              kWasmI32
            ],
            []))
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2, kExprLocalGet, 3,
      kExprLocalGet, 4, kGCPrefix, kExprArrayCopy, $u8arr, $u8arr
    ])
    .exportFunc();

const instance = builder.instantiate();
const {alloc_u8, alloc_ref, copy_u8} = instance.exports;

const dst = alloc_u8(64);
const src = alloc_u8(64);
const ref = alloc_ref(1);

// Control: copying between two genuine i8 arrays works fine.
copy_u8(dst, 0, src, 0, 64);
print('RESULT: control copy ok');

// Learn the raw canonical_element_type_ bit pattern used for a real
// externref array type directly from its own WasmTypeInfo.
const ref_map = getField(getPtr(ref), kMapOffset);
const ref_type_info = getField(ref_map, kMapTypeInfoOffset);
const ref_raw_element_type =
    getField(ref_type_info, kCanonicalElementTypeOffset);

// Corrupt the (shared, canonical) WasmTypeInfo for the i8 array *type* so
// that element_type() reports a ref type (is_ref() == true), while `src`'s
// Map still encodes the true i8 stride.
const src_map = getField(getPtr(src), kMapOffset);
const src_type_info = getField(src_map, kMapTypeInfoOffset);
setField(src_type_info, kCanonicalElementTypeOffset, ref_raw_element_type);

print(
    'RESULT: about to copy with corrupted (ref) element type -> ' +
    'expect crash');
copy_u8(dst, 0, src, 0, 64);
assertUnreachable('Process should have been killed.');
