// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const sig = makeSig([kWasmI32, kWasmI64, kWasmI64], [kWasmI64]);
builder.addFunction(undefined, sig)
  .addBody([
    kExprLocalGet, 2,
    kExprLocalGet, 1,
    kExprI64Shl,
]);
builder.instantiate();
