// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
builder.addFunction('test', kSig_i_iii)
    .addBodyWithEnd([
      kExprI32Const, 0,          // 0
      kExprI32Const, 0,          // 0, 0
      kExprI32Add,               // 0 + 0 -> 0
      kExprI32Const, 0,          // 0, 0
      kExprI32Const, 0,          // 0, 0, 0
      kExprI32Add,               // 0, 0 + 0 -> 0
      kExprDrop,                 // 0
      kExprDrop,                 // -
      kExprI32Const, 0,          // 0
      kExprI32Const, 0,          // 0, 0
      kExprI32Add,               // 0 + 0 -> 0
      kExprI32Const, 0,          // 0, 0
      kExprI32Const, 1,          // 0, 0, 1
      kExprI32Add,               // 0, 0 + 1 -> 1
      kExprBlock,    kWasmVoid,  // 0, 1
      kExprBr,       0,          // 0, 1
      kExprEnd,                  // 0, 1
      kExprI32Add,               // 0 + 1 -> 1
      kExprEnd
    ])
    .exportFunc();
var module = builder.instantiate();
assertEquals(1, module.exports.test());
