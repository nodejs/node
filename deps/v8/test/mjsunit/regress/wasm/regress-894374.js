// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32, false);
const sig = makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]);
builder.addFunction(undefined, sig)
  .addBodyWithEnd([
    kExprMemorySize, 0,
    kExprI32Const, 0,
    kExprI64Const, 0,
    kExprI64StoreMem8, 0, 0,
    kExprEnd,
  ]);
builder.addExport('main', 0);
builder.instantiate();  // shouldn't crash
