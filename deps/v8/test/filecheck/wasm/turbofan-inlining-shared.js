// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared --allow-natives-syntax --fuzzing
// Flags: --trace-turbo-inlining --turbofan --no-disable-optimizing-compilers
// Flags: --no-optimize-on-next-call-optimizes-to-maglev --no-turbolev
// Flags: --no-turbolev-future --no-stress-concurrent-inlining

d8.file.execute("test/mjsunit/mjsunit.js");
d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
const builder = new WasmModuleBuilder();
const struct_shared = builder.addStruct({
  fields: [makeField(kWasmI32, true)],
  shared: true
});
const struct_parent = builder.addStruct({
  fields: [makeField(wasmRefNullType(struct_shared), true)]
});
builder.addFunction("main", kSig_i_r) // x: externref
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprAnyConvertExtern,
    kGCPrefix, kExprRefCast, ...wasmUnsignedLeb(struct_parent),
    kGCPrefix, kExprStructGet, ...wasmUnsignedLeb(struct_parent), 0,
    kGCPrefix, kExprExternConvertAny,
    kGCPrefix, kExprAnyConvertExtern,
    kGCPrefix, kExprRefCast, ...wasmUnsignedLeb(struct_shared),
    kGCPrefix, kExprStructGet, ...wasmUnsignedLeb(struct_shared), 0
  ])
  .exportFunc();
builder.addFunction("make_parent", makeSig([wasmRefType(struct_shared)], [wasmRefType(struct_parent)]))
  .addBody([
     0, ...wasmUnsignedLeb(struct_parent)
  ])
builder.addFunction("make_shared", makeSig([], [wasmRefType(struct_shared)]))
  .addBody([
    kExprI32Const, 42,
    kGCPrefix, kExprStructNew, ...wasmUnsignedLeb(struct_shared)
  ])
const instance = builder.instantiate();
const main = instance.exports.main;
const parent = instance.exports.child;
function js_func() {
  return main();
}
for (let i = 0; i < 20; i++) {
  try { js_func(parent); } catch(e) {}
}

// CHECK: Considering Wasm function [0] main of module {{.*}} for inlining
// CHECK-NEXT: not inlining: module uses experimental 'shared' feature
%OptimizeFunctionOnNextCall(js_func);
assertTraps(kTrapIllegalCast, js_func);
