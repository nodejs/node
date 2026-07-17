// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let sig = makeSig(
  [kWasmS128, kWasmS128, kWasmS128, kWasmF64,  // Use up 7 param registers.
   kWasmS128, kWasmS128,  // Allocated by Liftoff into d8 through d11.
   kWasmS128,  // This will use d7, thinking it's still free.
   kWasmF64],  // This will get d7 as its linkage location.
  [kWasmF64]);

let func = builder.addFunction('crash', sig).addBody([
  kExprLocalGet, 7
]);

builder.addFunction('main', makeSig([], [kWasmF64])).exportFunc().addBody([
  ...wasmS128Const(Math.PI, Math.E),
  ...wasmS128Const(Math.PI, Math.E),
  ...wasmS128Const(Math.PI, Math.E),
  ...wasmF64Const(Infinity),
  ...wasmS128Const(Math.PI, Math.E),
  ...wasmS128Const(Math.PI, Math.E),
  ...wasmS128Const(Math.PI, Math.E),
  ...wasmF64Const(42),
  kExprCallFunction, func.index,
]);

assertEquals(42, builder.instantiate().exports.main());
