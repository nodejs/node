// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --wasm-in-js-inlining-opt --wasm-assert-types

// Bug: 525802228

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const struct_type = builder.addStruct([makeField(kWasmI64, true)]);

builder.addFunction("createStruct", makeSig([], [kWasmExternRef]))
  .addBody([
    kExprI64Const, 10,
    kGCPrefix, kExprStructNew, struct_type,
    kGCPrefix, kExprExternConvertAny
  ])
  .exportFunc();

builder.addFunction("overwrite", makeSig([kWasmExternRef], []))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprAnyConvertExtern,
    kGCPrefix, kExprRefCast, struct_type,
    kExprLocalGet, 0,
    kGCPrefix, kExprAnyConvertExtern,
    kGCPrefix, kExprRefCast, struct_type,
    kGCPrefix, kExprStructGet, struct_type, 0,
    kGCPrefix, kExprStructSet, struct_type, 0
  ])
  .exportFunc();

const exports = builder.instantiate().exports;
const struct_instance = exports.createStruct();

function test() {
  return exports.overwrite(struct_instance);
}

%PrepareFunctionForOptimization(test);
test();
%OptimizeFunctionOnNextCall(test);
test();
