// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.addMemory(1, 10);

builder.addFunction("crash", kSig_i_i)
  .exportFunc()
  .addLocals(kWasmI32, 10)
  .addBody([
    kExprBlock, kWasmVoid,
      kExprLoop, kWasmVoid,
        kExprLoop, kWasmVoid,
          kExprLocalGet, 1,
          kExprLocalGet, 2,
          kExprLocalGet, 3,
          kExprLocalGet, 4,
          kExprLocalGet, 5,
          kExprLocalGet, 6,
          kExprLocalGet, 7,
          kExprLocalGet, 8,
          kExprLocalGet, 9,
          kExprLocalGet, 10,
          kExprDrop,
          kExprDrop,
          kExprDrop,
          kExprDrop,
          kExprDrop,
          kExprDrop,
          kExprDrop,
          kExprDrop,
          kExprDrop,
          kExprDrop,
          kExprLocalGet, 0,
          kExprI32Const, 1,
          kExprI32Sub,
          kExprLocalTee, 0,
          kExprBrTable, 2, 2, 1, 0,
          kExprBr, 0,
        kExprEnd,  // loop
      kExprEnd,  // loop
    kExprEnd,  // block
    kExprLocalGet, 0,
])

let instance = builder.instantiate();
let result = instance.exports.crash(5);
console.log(result);
