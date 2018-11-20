// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-interpret-all

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
const sig = builder.addType(kSig_i_i);
builder.addFunction('call', kSig_i_v)
    .addBody([
      kExprI32Const, 0, kExprI32Const, 0, kExprCallIndirect, sig, kTableZero
    ])
    .exportAs('call');
builder.addImportedTable('imp', 'table');
const table = new WebAssembly.Table({element: 'anyfunc', initial: 1});
const instance = builder.instantiate({imp: {table: table}});
assertThrows(
    () => instance.exports.call(), WebAssembly.RuntimeError,
    /function signature mismatch/);
