// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// This test makes sure that by default, we do not compile with liftoff.

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('i32_add', kSig_i_ii)
    .addBody([kExprGetLocal, 0, kExprGetLocal, 1, kExprI32Add])
    .exportFunc();

const instance = builder.instantiate();

assertFalse(
    %IsLiftoffFunction(instance.exports.i32_add),
    'liftoff compilation should be off by default');
