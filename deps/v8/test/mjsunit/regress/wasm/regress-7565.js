// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
sig0 = makeSig([], [kWasmI32]);
builder.addFunction(undefined, sig0).addLocals(kWasmI64, 1).addBody([
  kExprLoop, kWasmI32,                    // loop i32
  kExprF32Const, 0x00, 0x00, 0x00, 0x00,  // f32.const 0      --> f32:0
  kExprLocalGet, 0x00,                    // get_local 0      --> i64:0
  kExprF32SConvertI64,                    // f32.sconvert/i64 --> f32:0
  kExprF32Ge,                             // f32.ge           --> i32:1
  kExprEnd,                               // end
]);
builder.addExport('main', 0);
const module = builder.instantiate();
assertEquals(1, module.exports.main());
