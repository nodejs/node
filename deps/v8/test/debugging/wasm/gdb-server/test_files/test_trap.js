// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();

builder.addMemory(32, 128).exportMemoryAs('mem')

var func_a_idx =
  builder.addFunction('wasm_A', kSig_v_v).addBody([
    kExprI32Const, 0,  // i32.const 0
    kExprI32Const, 42, // i32.const 42
    kExprI32StoreMem, 0, 0xff, 0xff, 0xff, 0xff, 0x0f, // i32.store offset = -1
  ]).index;

builder.addFunction('main', kSig_i_v).addBody([
    kExprCallFunction, func_a_idx, // call $wasm_A
    kExprI32Const, 0 // i32.const 0
  ])
  .exportFunc();

const instance = builder.instantiate();
const main_f = instance.exports.main;

function f() {
  var result = main_f();
  return result;
}
f();
