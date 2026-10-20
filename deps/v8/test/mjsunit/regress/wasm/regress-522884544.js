// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --trace-wasm-typer --turbofan
d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
const struct1 = builder.addStruct({
  fields: [makeField(kWasmI32, true)]
});

const struct_parent = builder.addStruct({
  fields: [
    makeField(wasmRefNullType(kWasmNullExternRef), true),
    makeField(wasmRefNullType(struct1), true)
  ]
});

builder.addFunction("get_null", makeSig([kWasmExternRef], [kWasmExternRef]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprAnyConvertExtern,
    kGCPrefix, kExprRefCast, ...wasmUnsignedLeb(struct_parent),
    kGCPrefix, kExprStructGet, ...wasmUnsignedLeb(struct_parent), 0
  ])
  .exportFunc();

builder.addFunction("get_struct", makeSig([kWasmExternRef], [kWasmExternRef]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprAnyConvertExtern,
    kGCPrefix, kExprRefCast, ...wasmUnsignedLeb(struct_parent),
    kGCPrefix, kExprStructGet, ...wasmUnsignedLeb(struct_parent), 1,
    kGCPrefix, kExprExternConvertAny
  ])
  .exportFunc();

builder.addFunction("process", makeSig([kWasmExternRef], [kWasmExternRef]))
  .addBody([
    kExprLocalGet, 0
  ])
  .exportFunc();

builder.addFunction("make_parent", makeSig([], [kWasmExternRef]))
  .addBody([
    kExprRefNull, kNullExternRefCode,
    kExprRefNull, struct1,
    kGCPrefix, kExprStructNew, ...wasmUnsignedLeb(struct_parent),
    kGCPrefix, kExprExternConvertAny
  ])
  .exportFunc();

const instance = builder.instantiate();

function test() {
  const get_null = instance.exports.get_null;
  const get_struct = instance.exports.get_struct;
  const process_func = instance.exports.process;
  const parent = instance.exports.make_parent();

  function js_func(parent, cond, cond2) {
    let x = get_null(parent);
    while (cond) {
      if (cond2) {
        x = process_func(x);
      } else {
        x = get_struct(parent);
      }
      cond = false;
    }
    return x;
  }

  %PrepareFunctionForOptimization(js_func);
  for (let i = 0; i < 4; i++) {
    js_func(parent, true, i % 2 === 0);
  }
  %OptimizeFunctionOnNextCall(js_func);
  js_func(parent, true, true);
}
test();
