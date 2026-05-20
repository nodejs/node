// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
builder.addFunction('test', kSig_v_v)
    .addBodyWithEnd([
      kExprI32Const,    0x0,   // const 0
      kExprI32Const,    0x0,   // const 0
      kExprBrIf,        0x00,  // br depth=0
      kExprLoop,        0x7f,  // loop i32
      kExprBlock,       0x7f,  // block i32
      kExprI32Const,    0x0,   // const 0
      kExprBr,          0x00,  // br depth=0
      kExprEnd,                // end
      kExprBr,          0x00,  // br depth=0
      kExprEnd,                // end
      kExprUnreachable,        // unreachable
      kExprEnd,                // end
    ])
    .exportFunc();
builder.instantiate();
