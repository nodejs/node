// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const sig = builder.addType(makeSig([kWasmI32], []));
builder.addFunction(undefined, sig)
  .addLocals({i64_count: 1})
  .addBody([
    kExprLoop, kWasmI32,
      kExprGetLocal, 1,
      kExprI64Const, 1,
      kExprLoop, kWasmI32,
        kExprGetLocal, 0,
        kExprI32Const, 1,
        kExprI32Const, 1,
        kExprIf, kWasmI32,
          kExprI32Const, 1,
        kExprElse,
          kExprUnreachable,
          kExprEnd,
        kExprSelect,
        kExprUnreachable,
        kExprEnd,
      kExprUnreachable,
      kExprEnd,
    kExprUnreachable
]);
builder.instantiate();
