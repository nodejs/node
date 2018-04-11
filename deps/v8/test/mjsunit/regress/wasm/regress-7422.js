// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
sig = makeSig([kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32], [kWasmI32]);
builder.addFunction(undefined, sig).addBody([kExprGetLocal, 4]);
builder.addMemory(16, 32);
builder.addFunction('main', sig)
    .addBody([
      kExprI32Const, 0, kExprSetLocal, 0,
      // Compute five arguments to the function call.
      kExprI32Const, 0, kExprI32Const, 0, kExprI32Const, 0, kExprI32Const, 0,
      kExprGetLocal, 4, kExprI32Const, 1, kExprI32Add,
      // Now some intermediate computation to force the arguments to be spilled
      // to the stack:
      kExprGetLocal, 0, kExprI32Const, 1, kExprI32Add, kExprGetLocal, 1,
      kExprGetLocal, 1, kExprI32Add, kExprI32Add, kExprDrop,
      // Now call the function.
      kExprCallFunction, 0
    ])
    .exportFunc();
var instance = builder.instantiate();
assertEquals(11, instance.exports.main(2, 4, 6, 8, 10));
