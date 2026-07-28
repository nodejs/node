// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction(undefined, kSig_v_v).addLocals(kWasmI64, 1).addBody([
  kExprI64Const, 0xeb,     0xd7, 0xaf, 0xdf,
  0xbe,          0xfd,     0xfa, 0xf5, 0x6b,  // i64.const
  kExprI32Const, 0,                           // i32.const
  kExprIf,       kWasmI32,                    // if i32
  kExprI32Const, 0,                           // i32.const
  kExprElse,                                  // else
  kExprI32Const, 0,                           // i32.const
  kExprEnd,                                   // end
  kExprBrIf,     0,                           // br_if depth=0
  kExprLocalSet, 0,                           // set_local 0
]);
builder.instantiate();
