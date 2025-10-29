// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --turbolev-inline-js-wasm-wrappers --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

var builder = new WasmModuleBuilder();
let array_type_index = builder.addArray(kWasmI32, true);

builder.addFunction('createArray', makeSig([kWasmI32], [kWasmExternRef]))
.addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprArrayNewDefault, array_type_index,
    kGCPrefix, kExprExternConvertAny])
.exportFunc();

builder.addFunction("arrayLen", makeSig([kWasmExternRef], [kWasmI32]))
.addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprAnyConvertExtern,
    kGCPrefix, kExprRefCast, array_type_index,  // invalid cast
    kGCPrefix, kExprArrayLen,
])
.exportFunc();

var instance = builder.instantiate({});

function __f_0(__v_1, __v_2) {
  %PrepareFunctionForOptimization(__v_2);
  __v_1();
  %OptimizeFunctionOnNextCall(__v_2);
  __v_1();
}

let array = instance.exports.createArray();
let __v_8 = (() => (arr) => {
  try {
    instance.exports.arrayLen(arr);
  } catch (e) {}
})();
__f_0(() => __v_8(array), __v_8);
__v_8();
