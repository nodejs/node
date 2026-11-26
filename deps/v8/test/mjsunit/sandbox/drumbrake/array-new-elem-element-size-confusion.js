// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless --sandbox-testing

// Regression test: array.new_elem validates the array element layout against
// the trusted array type. Expected to abort via SBXCHECK.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Same corruption technique/offset as
// sandbox/drumbrake/array-alloc-element-size-confusion.js.
const kHeapObjectTag = 1;
const kMapOffset = 0;
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
const $funcarr = builder.addArray(kWasmFuncRef, {mutability: true});
const elemFn =
    builder.addFunction('elemFn', kSig_i_v).addBody([kExprI32Const, 0]);
const segment = builder.addPassiveElementSegment(
    [[kExprRefFunc, elemFn.index]], kWasmFuncRef);

builder
    .addFunction(
        'new_elem', makeSig([kWasmI32, kWasmI32], [wasmRefType($funcarr)]))
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1, kGCPrefix, kExprArrayNewElem,
      $funcarr, segment
    ])
    .exportFunc();

const instance = builder.instantiate();
const {new_elem} = instance.exports;

// Control: array.new_elem on a genuine funcref array type works fine.
const control = new_elem(0, 1);
print('RESULT: control new_elem ok');

// Corrupt the shared, canonical Map's WasmByte1 (the tagged-pointer stride
// for a ref array) from its real value down to 1, while array.new_elem's
// caller-trusted element size stays the same.
const map = getField(getPtr(control), kMapOffset);
const real_element_size = getByte(map, kMapWasmByte1Offset);
print('RESULT: real element size encoded in Map = ' + real_element_size);
setByte(map, kMapWasmByte1Offset, 1);

print(
    'RESULT: about to array.new_elem with corrupted Map element size -> ' +
    'expect crash');
const corrupted = new_elem(0, 1);
assertUnreachable('Process should have been killed.');
