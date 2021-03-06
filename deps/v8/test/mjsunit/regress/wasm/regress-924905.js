// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.addFunction("kaboom", kSig_i_v)
    .addBody([
      kExprI32Const, 0,
      kExprI32Const, 0,
      kExprI32And,
      kExprI32Const, 0,
      kExprI32ShrU,
    ]).exportFunc();
let instance = builder.instantiate();
assertEquals(0, instance.exports.kaboom());
