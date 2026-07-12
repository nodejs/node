// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-sparkplug --expose-gc
// Flags: --trace-turbo-inlining

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestInliningI64Struct() {
  let builder = new WasmModuleBuilder();
  let struct = builder.addStruct([makeField(kWasmI64, true)]);

  builder.addFunction('createStruct', makeSig([], [kWasmExternRef]))
    .addBody([
      ...wasmI64Const(Number.MAX_SAFE_INTEGER),
      kGCPrefix, kExprStructNew, struct,
      kGCPrefix, kExprExternConvertAny,
    ])
    .exportFunc();

  builder.addFunction('overwrite', makeSig([kWasmExternRef], []))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, struct,
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, struct,
      kGCPrefix, kExprStructGet, struct, 0,
      kGCPrefix, kExprStructSet, struct, 0,
    ])
    .exportFunc();

  let instance = builder.instantiate({});
  let wasm = instance.exports;

  let structVal = wasm.createStruct();
  let overwrite = () => wasm.overwrite(structVal);
  %PrepareFunctionForOptimization(overwrite);
  overwrite();
  %OptimizeFunctionOnNextCall(overwrite);
  overwrite();
  assertOptimized(overwrite);
})();

(function TestInliningI64Array() {
  let builder = new WasmModuleBuilder();
  let array = builder.addArray(kWasmI64, true);

  builder.addFunction('createArray', makeSig([], [kWasmExternRef]))
    .addBody([
      ...wasmI64Const(Number.MAX_SAFE_INTEGER),
      kExprI32Const, 1,
      kGCPrefix, kExprArrayNew, array,
      kGCPrefix, kExprExternConvertAny,
    ])
    .exportFunc();

  builder.addFunction('overwrite', makeSig([kWasmExternRef, kWasmI32], []))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, array,
      kExprLocalGet, 1,
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, array,
      kExprLocalGet, 1,
      kGCPrefix, kExprArrayGet, array,
      kGCPrefix, kExprArraySet, array,
    ])
    .exportFunc();

  let instance = builder.instantiate({});
  let wasm = instance.exports;

  let arrayVal = wasm.createArray();
  let overwrite = () => wasm.overwrite(arrayVal);
  %PrepareFunctionForOptimization(overwrite);
  overwrite();
  %OptimizeFunctionOnNextCall(overwrite);
  overwrite();
  assertOptimized(overwrite);
})();
