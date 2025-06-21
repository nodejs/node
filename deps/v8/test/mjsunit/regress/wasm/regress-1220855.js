// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging --liftoff --no-wasm-tier-up

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1, true);
builder.addFunction('test', makeSig([kWasmI32], [kWasmI64]))
    .addBody([
      ...wasmI32Const(1 << 30),  // pages to grow
      kExprMemoryGrow, 0,        // returns -1
      kExprI64UConvertI32        // asserts that the i32 is zero extended
    ])
    .exportFunc();
const instance = builder.instantiate();
instance.exports.test();
