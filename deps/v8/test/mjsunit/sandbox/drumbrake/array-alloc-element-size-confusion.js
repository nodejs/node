// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless --sandbox-testing

// Regression test: array allocation validates the element size of its Map
// against the trusted array type. Expected to abort via SBXCHECK.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Prepare corruption utilities (same technique/offsets as
// sandbox/drumbrake/array-element-size-confusion.js and
// sandbox/liftoff-wasmarray-i64-indexing.js).
const kHeapObjectTag = 1;
const kMapOffset = 0;
// Map::inobject_properties_start_or_constructor_function_index_, aka
// Map::WasmByte1() -- the byte that encodes a WasmArray's per-element byte
// stride, used by WasmArray::ElementAddress()/element_offset()/SizeFor().
const kMapWasmByte1Offset = 0x5;

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
function getPtr(obj) {
  return Sandbox.getAddressOf(obj) + kHeapObjectTag;
}
function getField(obj, offset) {
  return memory.getUint32(obj + offset - kHeapObjectTag, true);
}
function getByte(obj, offset) {
  return memory.getUint8(obj + offset - kHeapObjectTag);
}
function setByte(obj, offset, value) {
  memory.setUint8(obj + offset - kHeapObjectTag, value);
}

const builder = new WasmModuleBuilder();
const $s128arr = builder.addArray(kWasmS128, {mutability: true});

builder.addFunction('alloc_s128', makeSig([kWasmI32], [wasmRefType($s128arr)]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprArrayNewDefault, $s128arr])
    .exportFunc();

const instance = builder.instantiate();
const {alloc_s128} = instance.exports;

// Control: a genuine s128 array allocates and initializes fine.
const control = alloc_s128(64);
print('RESULT: control alloc ok');

// Corrupt the shared, canonical Map's WasmByte1 from 16 (s128) down to 1,
// while array.new_default's caller-trusted element size stays 16.
const map = getField(getPtr(control), kMapOffset);
const real_element_size = getByte(map, kMapWasmByte1Offset);
print('RESULT: real element size encoded in Map = ' + real_element_size);
setByte(map, kMapWasmByte1Offset, 1);

print(
    'RESULT: about to allocate with corrupted Map element size -> ' +
    'expect crash');
const corrupted = alloc_s128(64);
assertUnreachable('Process should have been killed.');
