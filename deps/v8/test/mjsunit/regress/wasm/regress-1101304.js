// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addType(makeSig(
    [kWasmI64, kWasmI32, kWasmF64, kWasmI64, kWasmI64, kWasmI32, kWasmI64],
    []));
builder.addFunction(undefined, 0 /* sig */).addBody([
  kExprI32Const,    0,          // i32.const
  kExprIf,          kWasmVoid,  // if @3
  kExprI32Const,    1,          // i32.const
  kExprIf,          kWasmVoid,  // if @7
  kExprNop,                     // nop
  kExprElse,                    // else @10
  kExprUnreachable,             // unreachable
  kExprEnd,                     // end @12
  kExprLocalGet,    0x06,       // local.get
  kExprLocalSet,    0x00,       // local.set
  kExprLocalGet,    0x03,       // local.get
  kExprLocalGet,    0x00,       // local.get
  kExprI64Sub,                  // i64.sub
  kExprDrop,                    // drop
  kExprUnreachable,             // unreachable
  kExprEnd                      // end @24
]);
builder.toModule();
