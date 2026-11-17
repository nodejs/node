// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-in-js-inlining-body
// Flags: --wasm-in-js-inlining-opt --turbolev

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('test_func', makeSig([kWasmExternRef], [kWasmI32]))
  .addLocals(wasmRefType(kWasmAnyRef), 1)
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprAnyConvertExtern,
    kGCPrefix, kExprRefCast, kAnyRefCode,
    kExprLocalTee, 1,
    kGCPrefix, kExprRefCast, kStructRefCode,
    kExprDrop,
    kExprI32Const, 0,
  ])
  .exportFunc();

const instance = builder.instantiate();
const test_func = instance.exports.test_func;

function js_fn(x) {
  return test_func(x);
}

%PrepareFunctionForOptimization(js_fn);
assertTraps(kTrapIllegalCast, () => js_fn(null));
%OptimizeFunctionOnNextCall(js_fn);
assertTraps(kTrapIllegalCast, () => js_fn(null));
