// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.startRecGroup();
let $array0 = builder.addArray(kWasmI8, {final: true});
builder.endRecGroup();
let $struct2 = builder.addStruct(
  {fields: [makeField(kWasmStructRef, true)]});
let $struct3 = builder.addStruct(
  {fields: [makeField(kWasmStructRef, true)], supertype: $struct2});
let $struct4 = builder.addStruct(
  {fields: [makeField(kWasmStructRef, true),
            makeField(kWasmI64, false),
            makeField(kWasmI8, false)],
   supertype: $struct3});
let $array6 = builder.addArray(wasmRefNullType($struct3));

let $func19 = builder.addFunction("main", kSig_v_v)
  .addBody([
    kExprRefNull, kAnyRefCode,
    kGCPrefix, kExprRefCastNull, $array0,
    kGCPrefix, kExprRefCastNull, $array6,
    ...wasmI32Const(826276),
    kExprRefNull, $struct3,
    ...wasmI32Const(1992565),
    kGCPrefix, kExprArrayFill, $array6,
  ]).exportFunc();

const instance = builder.instantiate({});
assertTraps(kTrapNullDereference, () => instance.exports.main());
