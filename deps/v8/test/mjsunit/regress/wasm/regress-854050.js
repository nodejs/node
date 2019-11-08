// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction(undefined, makeSig([kWasmI32, kWasmF32], []))
    .addLocals({i32_count: 7})
    .addBody([
      kExprLocalGet,    0,          // get_local
      kExprI32Const,    0,          // i32.const 0
      kExprIf,          kWasmStmt,  // if
      kExprUnreachable,             // unreachable
      kExprEnd,                     // end if
      kExprLocalGet,    4,          // get_local
      kExprLocalTee,    8,          // tee_local
      kExprBrIf,        0,          // br_if depth=0
      kExprLocalTee,    7,          // tee_local
      kExprLocalTee,    0,          // tee_local
      kExprLocalTee,    2,          // tee_local
      kExprLocalTee,    8,          // tee_local
      kExprDrop,                    // drop
      kExprLoop,        kWasmStmt,  // loop
      kExprEnd,                     // end loop
    ]);
builder.instantiate();
