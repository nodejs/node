// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-reftypes --trace-turbo-graph

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();

builder.addFunction(`get`, makeSig([], [kWasmExternRef]))
    .addBody([kExprTableGet])
    .exportFunc();
builder.addFunction(`fill`, makeSig([kWasmI32, kWasmAnyFunc, kWasmI32], []))
    .addBody([])
    .exportFunc();
try {
  builder.toModule();
} catch {}
