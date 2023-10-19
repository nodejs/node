// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --liftoff --nowasm-tier-up
//
// This test is intended to make Liftoff generate code that uses a very large
// stack frame, and then try to call another function (which would write to the
// stack pointer location). On Windows, large frames need extra code to touch
// every page in order, because the OS only leaves a small guard area for the
// stack, and trying to access past that area, even into memory that was
// intentionally reserved for this thread's stack, will crash the program.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();

var func_idx = builder.addFunction('helper', kSig_i_v)
    .addLocals(kWasmI32, 1)
    .addBody([
        kExprI32Const, 0x01,
    ]).index;

var large_function_body = [];
const num_temporaries = 16 * 1024;
for (let i = 0; i < num_temporaries; ++i) {
  large_function_body.push(kExprCallFunction, func_idx);
}
for (let i = 1; i < num_temporaries; ++i) {
  large_function_body.push(kExprI32Add);
}

builder.addFunction('test', kSig_i_v)
    .addBody(large_function_body)
    .exportFunc();
var module = builder.instantiate();

assertEquals(num_temporaries, module.exports.test());
