// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --experimental-wasm-shared
d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
const struct0 = builder.addStruct({
  fields: [makeField(kWasmI32, true)],
  shared: true
});

const struct_parent = builder.addStruct({
  fields: [makeField(wasmRefNullType(kWasmNullRef).shared(), true)]
});

builder.addFunction("main", kSig_i_r)
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprAnyConvertExtern,
    kGCPrefix, kExprRefCast, ...wasmUnsignedLeb(struct_parent),
    kGCPrefix, kExprStructGet, ...wasmUnsignedLeb(struct_parent), 0,
    kGCPrefix, kExprStructGet, ...wasmUnsignedLeb(struct0), 0])
  .exportFunc();

builder.addFunction("make_parent", makeSig([], [wasmRefType(struct_parent)]))
  .addBody([
    kExprRefNull, kWasmSharedTypeForm, kNullRefCode,
    kGCPrefix, kExprStructNew, ...wasmUnsignedLeb(struct_parent)
  ])
  .exportFunc();

const instance = builder.instantiate();

function test() {
  const main = instance.exports.main;
  const parent = instance.exports.make_parent();

  function js_func(x) {
    return main(x);
  }

  %PrepareFunctionForOptimization(js_func);
  for (let i = 0; i < 20; i++) {
    try { js_func(parent); } catch(e) {}
  }
  %OptimizeFunctionOnNextCall(js_func);
  try { js_func(null); } catch(e) {}
}
test();
