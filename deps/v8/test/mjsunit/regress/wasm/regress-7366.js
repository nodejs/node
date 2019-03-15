// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction(undefined, kSig_i_iii).addBody([
  // Return the sum of all arguments.
  kExprGetLocal, 0, kExprGetLocal, 1, kExprGetLocal, 2, kExprI32Add, kExprI32Add
]);
const sig = builder.addType(kSig_i_iii);
builder.addFunction(undefined, kSig_i_iii)
    .addBody([
      ...wasmI32Const(1),         // i32.const 0x1
      kExprSetLocal, 0,           // set_local 0
      ...wasmI32Const(4),         // i32.const 0x1
      kExprSetLocal, 1,           // set_local 1
      ...wasmI32Const(16),        // i32.const 0x1
      kExprSetLocal, 2,           // set_local 2
      kExprLoop, kWasmStmt,       // loop
      kExprEnd,                   // end
      kExprGetLocal, 0,           // get_local 0
      kExprGetLocal, 1,           // get_local 1
      kExprGetLocal, 2,           // get_local 2
      kExprI32Const, 0,           // i32.const 0 (func index)
      kExprCallIndirect, sig, 0,  // call indirect
    ])
    .exportAs('main');
builder.appendToTable([0]);
const instance = builder.instantiate();
assertEquals(21, instance.exports.main());
