// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

function testOptimized(run, fctToOptimize) {
  %PrepareFunctionForOptimization(fctToOptimize);
  for (let i = 0; i < 10; ++i) {
    run();
  }
  %OptimizeFunctionOnNextCall(fctToOptimize);
  for (let i = 0; i < 10; ++i) {
    run();
  }
}

(function TestInliningStructGet() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let struct = builder.addStruct([makeField(kWasmI32, true)]);

  builder.addFunction('createStructNull', makeSig([kWasmI32], [kWasmExternRef]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStructNew, struct,
      kGCPrefix, kExprExternExternalize,
    ])
    .exportFunc();

  builder.addFunction('getElementNull', makeSig([kWasmExternRef], [kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprExternInternalize,
      kGCPrefix, kExprRefCast, struct,
      kGCPrefix, kExprStructGet, struct, 0])
    .exportFunc();

  builder.addFunction('createStruct',
                      makeSig([kWasmI32], [wasmRefType(kWasmExternRef)]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStructNew, struct,
      kGCPrefix, kExprExternExternalize,
    ])
    .exportFunc();

  builder.addFunction('getElement',
                      makeSig([wasmRefType(kWasmExternRef)], [kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprExternInternalize,
      kGCPrefix, kExprRefCast, struct,
      kGCPrefix, kExprStructGet, struct, 0])
    .exportFunc();

  let instance = builder.instantiate({});
  let wasm = instance.exports;
  let n = 100;

  for (let [create, get] of [
      [wasm.createStruct, wasm.getElement],
      [wasm.createStructNull, wasm.getElementNull]]) {
    let fct = () => {
      let res = 0;
      for (let i = 1; i <= n; ++i) {
        const struct = create(i);
        res += get(struct);
      }
      return res;
    };
    testOptimized(() => assertEquals((n * n + n) / 2, fct()), fct);

    let getNull = () => get(null);
    let fctNullValue = () => {
      for (let i = 1; i <= n; ++i) {
        // Depending on the param type (ref / ref.null), either the wrapper or
        // the cast inside the function will throw.
        assertThrows(getNull);
      }
    };
    testOptimized(fctNullValue, getNull);
  }
})();
