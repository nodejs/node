// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const sig = builder.addType(makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]));
builder.addFunction(undefined, sig)
  .addBody([
    kExprGetLocal, 2,
    kExprIf, kWasmStmt,
      kExprBlock, kWasmStmt
  ]);
builder.addExport('main', 0);
assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
