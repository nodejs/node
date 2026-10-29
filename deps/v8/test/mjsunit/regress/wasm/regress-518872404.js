// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();

let $struct0 = builder.addStruct({
  fields: [
    makeField(kWasmI32, true),
    makeField(kWasmI64, true),
    makeField(wasmRefNullType(kWasmEqRef).shared(), true)
  ],
  shared: true
});

let $array1 = builder.addArray(
  wasmRefNullType(kWasmEqRef).shared(), {shared: true});
let $sig2 = builder.addType(kSig_i_v);

let run_test = builder.addFunction(undefined, $sig2).exportAs('run_test')
.addLocals(wasmRefType($struct0), 2)  // $var0 - $var1
.addLocals(wasmRefType($array1), 1)  // $var2
.addBody([
  kExprI32Const, 1,
  kExprI64Const, 10,
  kExprRefNull, kWasmSharedTypeForm, kEqRefCode,
  kGCPrefix, kExprStructNew, $struct0,
  kExprLocalSet, 0,  // $var0
  kExprI32Const, 2,
  kExprI64Const, 20,
  kExprRefNull, kWasmSharedTypeForm, kEqRefCode,
  kGCPrefix, kExprStructNew, $struct0,
  kExprLocalSet, 1,  // $var1
  kExprLocalGet, 0,  // $var0
  kGCPrefix, kExprArrayNewFixed, $array1, 1,
  kExprLocalSet, 2,  // $var2
  kExprLocalGet, 2,  // $var2
  kExprI32Const, 0,
  kAtomicPrefix, kExprArrayAtomicGet, kAtomicSeqCst, $array1,
  kExprLocalGet, 0,  // $var0
  kExprRefEq,
]);

const instance = builder.instantiate();
assertEquals(1, instance.exports.run_test());
