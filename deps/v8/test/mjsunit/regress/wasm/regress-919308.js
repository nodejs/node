// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction(undefined, kSig_i_i)
  .addLocals(kWasmI32, 5)
  .addBody([
    kExprLocalGet, 0,    // --> 1
    kExprIf, kWasmI32,
      kExprLocalGet, 0,  // --> 1
    kExprElse,
      kExprUnreachable,
      kExprEnd,
    kExprIf, kWasmI32,
      kExprLocalGet, 0,  // --> 1
    kExprElse,
      kExprUnreachable,
      kExprEnd,
    kExprIf, kWasmI32,
      kExprI32Const, 0,
      kExprLocalGet, 0,
      kExprI32Sub,       // --> -1
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprI32Sub,       // --> 0
      kExprI32Sub,       // --> -1
    kExprElse,
      kExprUnreachable,
      kExprEnd
]);
builder.addExport('main', 0);
const instance = builder.instantiate();
assertEquals(-1, instance.exports.main(1));
