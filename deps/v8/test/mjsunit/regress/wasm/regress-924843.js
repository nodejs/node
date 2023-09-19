// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const sig = builder.addType(makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]));
builder.addFunction(undefined, sig)
  .addBody([
    kExprLocalGet, 2,
    kExprIf, kWasmVoid,
      kExprBlock, kWasmVoid
  ]);
builder.addExport('main', 0);
assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
