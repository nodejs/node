// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
builder.addFunction('test', kSig_i_iii)
    .addBodyWithEnd([
      kExprI32Const, 0x00,  // i32.const 0
      kExprI32Const, 0x00,  // i32.const 0
      kExprI32Add,          // i32.add -> 0
      kExprI32Const, 0x00,  // i32.const 0
      kExprI32Const, 0x00,  // i32.const 0
      kExprI32Add,          // i32.add -> 0
      kExprI32Add,          // i32.add -> 0
      kExprI32Const, 0x01,  // i32.const 1
      kExprI32Const, 0x00,  // i32.const 0
      kExprI32Add,          // i32.add -> 1
      kExprBlock,    0x7f,  // @39 i32
      kExprI32Const, 0x00,  // i32.const 0
      kExprBr,       0x00,  // depth=0
      kExprEnd,             // @90
      kExprI32Add,          // i32.add -> 1
      kExprI32Add,          // i32.add -> 1
      kExprEnd
    ])
    .exportFunc();
var module = builder.instantiate();
assertEquals(1, module.exports.test());
