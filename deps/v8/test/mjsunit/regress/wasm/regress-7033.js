// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
builder.addFunction('test', kSig_i_iii)
    .addBodyWithEnd([
      kExprI32Const, 0x07,  // i32.const 7
      kExprI32Const, 0x00,  // i32.const 0
      kExprI32Const, 0x00,  // i32.const 0
      kExprI32And,          // i32.and
      kExprI32And,          // i32.and
      kExprEnd,             // -
    ])
    .exportFunc();
var module = builder.instantiate();
assertEquals(0, module.exports.test());
