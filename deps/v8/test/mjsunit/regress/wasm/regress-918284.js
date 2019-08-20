// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction(undefined, kSig_i_i)
  .addLocals({i32_count: 7})
  .addBody([
    kExprI32Const, 0,
    kExprIf, kWasmI32,   // @11 i32
      kExprI32Const, 0,
    kExprElse,   // @15
      kExprI32Const, 1,
      kExprEnd,   // @18
    kExprTeeLocal, 0,
    kExprI32Popcnt
]);
builder.instantiate();
