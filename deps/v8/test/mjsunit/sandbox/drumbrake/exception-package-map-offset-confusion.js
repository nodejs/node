// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless --sandbox-testing

// Regression test: reading the tag and values of an exception package
// validates the Map-derived in-object offsets. Expected to abort via SBXCHECK.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Prepare corruption utilities (same technique/offsets as
// sandbox/drumbrake/array-alloc-element-size-confusion.js).
const kHeapObjectTag = 1;
const kMapOffset = 0;
// Map::inobject_properties_start_or_constructor_function_index_ -- also used
// by WasmExceptionPackage::New/GetExceptionTagField/GetExceptionValuesField
// to locate the package's "tag" and "values" in-object fields.
const kMapInObjectPropsStartOffset = 0x5;

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
const except = builder.addTag(kSig_v_i);
builder.addExportOfKind('except_tag', kExternalTag, except);
const thrower_index = builder.addImport('m', 'thrower', kSig_v_v);
builder.addFunction('call_thrower_catch', kSig_i_v)
    .addBody([
      kExprTry, kWasmI32,
        kExprCallFunction, thrower_index,
        kExprI32Const, 2,
      kExprCatch, except,
        kExprReturn,
      kExprEnd,
    ])
    .exportFunc();

// The exception is created before the Map is corrupted, so that throwing it
// from JS does not go through WasmExceptionPackage::New. The Wasm catch then
// has to read its tag and values via GetExceptionTagField and
// GetExceptionValuesField.
let thrown;
const instance = builder.instantiate({
  m: {
    thrower: () => {
      throw thrown;
    }
  }
});
thrown = new WebAssembly.Exception(instance.exports.except_tag, [42]);

// Control: a genuine throw/catch round-trips the value through
// GetExceptionTagField / GetExceptionValuesField (catch-match and decode)
// just fine.
assertEquals(42, instance.exports.call_thrower_catch());
print('RESULT: control throw/catch ok');

const map = getField(getPtr(thrown), kMapOffset);
const real_value = getByte(map, kMapInObjectPropsStartOffset);
print('RESULT: real inobject_properties_start = ' + real_value);

// Corrupt the shared, canonical Map used for every WasmExceptionPackage.
setByte(map, kMapInObjectPropsStartOffset, 200);

print(
    'RESULT: about to catch with corrupted exception-package Map -> ' +
    'expect crash');
assertEquals(42, instance.exports.call_thrower_catch());
assertUnreachable('Process should have been killed.');
