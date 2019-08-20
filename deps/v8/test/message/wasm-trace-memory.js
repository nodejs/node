// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-stress-opt --trace-wasm-memory --no-liftoff --no-future
// Flags: --no-wasm-tier-up

load("test/mjsunit/wasm/wasm-module-builder.js");

var builder = new WasmModuleBuilder();
builder.addMemory(1);
builder.addFunction('load', kSig_v_i)
    .addBody([kExprGetLocal, 0, kExprI32LoadMem, 0, 0, kExprDrop])
    .exportFunc();
builder.addFunction('load8', kSig_v_i)
    .addBody([kExprGetLocal, 0, kExprI32LoadMem8U, 0, 0, kExprDrop])
    .exportFunc();
builder.addFunction('loadf', kSig_v_i)
    .addBody([kExprGetLocal, 0, kExprF32LoadMem, 0, 0, kExprDrop])
    .exportFunc();
builder.addFunction('store', kSig_v_ii)
    .addBody([kExprGetLocal, 0, kExprGetLocal, 1, kExprI32StoreMem, 0, 0])
    .exportFunc();
builder.addFunction('store8', kSig_v_ii)
    .addBody([kExprGetLocal, 0, kExprGetLocal, 1, kExprI32StoreMem8, 0, 0])
    .exportFunc();
var module = builder.instantiate();

module.exports.load(4);
module.exports.load8(1);
module.exports.store(4, 0x12345678);
module.exports.load(2);
module.exports.load8(6);
module.exports.loadf(2);
module.exports.store8(4, 0xab);
module.exports.load(2);
module.exports.loadf(2);
