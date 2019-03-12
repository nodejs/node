// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32, false);
builder.addFunction('main', kSig_i_v)
    .addBody([
      ...wasmI32Const(10000),  // i32.const 10000
      kExprMemoryGrow, 0,      // grow_memory --> -1
      kExprI32Popcnt,          // i32.popcnt  --> 32
    ])
    .exportFunc();
const instance = builder.instantiate();
assertEquals(32, instance.exports.main());
