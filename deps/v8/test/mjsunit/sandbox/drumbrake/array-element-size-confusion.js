// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless --sandbox-testing

// Regression test: array.copy validates the element layout of both arrays
// against their trusted array types. Expected to abort via SBXCHECK.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Prepare corruption utilities (same technique/offsets as
// sandbox/liftoff-wasmarray-i64-indexing.js).
const kHeapObjectTag = 1;
const kMapOffset = 0;
// Map::constructor_or_back_pointer_or_native_context_ (holds the
// WasmTypeInfo for Wasm GC Maps).
const kMapTypeInfoOffset = 0x14;
// WasmTypeInfo::canonical_element_type_ (uint32_t, after the Map pointer and
// canonical_type_ fields).
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

builder.addFunction('alloc_u8', makeSig([kWasmI32], [wasmRefType($u8arr)]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprArrayNewDefault, $u8arr])
    .exportFunc();
builder.addFunction('alloc_s128', makeSig([kWasmI32], [wasmRefType($s128arr)]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprArrayNewDefault, $s128arr])
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
const {alloc_u8, alloc_s128, copy_u8} = instance.exports;

const dst = alloc_u8(64);
const src = alloc_u8(64);
const s128 = alloc_s128(1);

// Control: copying between two genuine i8 arrays works fine.
copy_u8(dst, 0, src, 0, 64);
print('RESULT: control copy ok');

// Learn the raw canonical_element_type_ bit pattern used for a real s128
// array type directly from its own WasmTypeInfo, rather than hand-decoding
// the bitfield format.
const s128_map = getField(getPtr(s128), kMapOffset);
const s128_type_info = getField(s128_map, kMapTypeInfoOffset);
const s128_raw_element_type =
    getField(s128_type_info, kCanonicalElementTypeOffset);

// Corrupt the (shared, canonical) WasmTypeInfo for the i8 array *type* so
// that element_type() reports s128 elements, while `dst`'s Map still
// encodes the true i8 stride.
const dst_map = getField(getPtr(dst), kMapOffset);
const dst_type_info = getField(dst_map, kMapTypeInfoOffset);
setField(dst_type_info, kCanonicalElementTypeOffset, s128_raw_element_type);

print('RESULT: about to copy with corrupted element type -> expect crash');
copy_u8(dst, 0, src, 0, 64);
assertUnreachable('Process should have been killed.');
