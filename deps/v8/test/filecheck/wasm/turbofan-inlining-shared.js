// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared --allow-natives-syntax --fuzzing
// Flags: --trace-turbo-inlining --turbofan --no-disable-optimizing-compilers
// Flags: --no-optimize-on-next-call-optimizes-to-maglev --no-turbolev
// Flags: --no-turbolev-future --no-stress-concurrent-inlining
// Flags: --no-stress-background-compile --turbo-filter=js_*

d8.file.execute("test/mjsunit/mjsunit.js");
d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
%NeverOptimizeFunction(assertThrows);
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
  .exportFunc();
const instance = builder.instantiate();
const main = instance.exports.main;
const parent = instance.exports.child;
function js_func() {
  return main(undefined);
}
%PrepareFunctionForOptimization(js_func);
for (let i = 0; i < 20; i++) {
  try { js_func(parent); } catch(e) {}
}

// CHECK: Considering Wasm function [0] main of module {{.*}} for inlining
// CHECK-NEXT: not inlining: experimental 'shared' feature is enabled
%OptimizeFunctionOnNextCall(js_func);
assertTraps(kTrapIllegalCast, js_func);

// Module using only an abstract shared heap type (no shared struct/array/table
// declarations) still detects 'shared' and bails out.
const b2 = new WasmModuleBuilder();
b2.addGlobal(wasmRefNullType(kWasmI31Ref).shared(), false, false,
             [...wasmI32Const(7), kGCPrefix, kExprRefI31Shared]);
b2.addFunction("get_shared_i31", kSig_i_v)
  .addBody([kExprGlobalGet, 0, kGCPrefix, kExprI31GetS])
  .exportFunc();
const get_shared_i31 = b2.instantiate().exports.get_shared_i31;
function js_shared_i31() { return get_shared_i31(); }
%PrepareFunctionForOptimization(js_shared_i31);
assertEquals(7, js_shared_i31());
assertEquals(7, js_shared_i31());
// CHECK: Considering Wasm function [0] get_shared_i31 of module {{.*}} for inlining
// CHECK-NEXT: not inlining: experimental 'shared' feature is enabled
%OptimizeFunctionOnNextCall(js_shared_i31);
assertEquals(7, js_shared_i31());

// Unshared module: legacy TurboFan bails out whenever --wasm-shared is enabled
// (while Turboshaft in turboshaft-inlining-shared.js still inlines it), and
// traps with kTrapIllegalCast when passed a runtime shared object via externref.
const b3 = new WasmModuleBuilder();
const struct_unshared = b3.addStruct([makeField(kWasmI32, true)]);
b3.addFunction("make_unshared", makeSig([kWasmI32], [kWasmExternRef]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructNew, struct_unshared,
            kGCPrefix, kExprExternConvertAny])
  .exportFunc();
b3.addFunction("get_unshared", kSig_i_r)
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprAnyConvertExtern,
            kGCPrefix, kExprRefCast, struct_unshared,
            kGCPrefix, kExprStructGet, struct_unshared, 0])
  .exportFunc();
const unshared = b3.instantiate().exports;
let cast_arg = unshared.make_unshared(11);
function js_unshared() { return unshared.get_unshared(cast_arg); }
%PrepareFunctionForOptimization(js_unshared);
assertEquals(11, js_unshared());
assertEquals(11, js_unshared());
// CHECK: Considering Wasm function [1] get_unshared of module {{.*}} for inlining
// CHECK-NEXT: not inlining: experimental 'shared' feature is enabled
%OptimizeFunctionOnNextCall(js_unshared);
assertEquals(11, js_unshared());
cast_arg = instance.exports.make_shared();
assertTraps(kTrapIllegalCast, js_unshared);
