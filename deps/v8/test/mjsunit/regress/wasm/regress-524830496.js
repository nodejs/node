// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --experimental-wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
const struct_parent = builder.addStruct({
  fields: [makeField(wasmRefNullType(kWasmNullRef).shared(), true)]
});
builder.addFunction("main", kSig_i_r)
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprAnyConvertExtern,
    kGCPrefix, kExprRefCast, ...wasmUnsignedLeb(struct_parent),
    kGCPrefix, kExprStructGet, ...wasmUnsignedLeb(struct_parent), 0,
    kGCPrefix, kExprArrayLen])
  .exportFunc();
const instance = builder.instantiate();
const main = instance.exports.main;
function js_func() {
  return main();
}
%PrepareFunctionForOptimization(js_func);
for (let i = 0; i < 20; i++) {
  assertTraps(kTrapIllegalCast, js_func);
}
%OptimizeFunctionOnNextCall(js_func);
assertTraps(kTrapIllegalCast, js_func);
