// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-eh

load('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
let except = builder.addException(kSig_v_i);
builder.addFunction("throw0", kSig_v_v)
    .addBody([
      kExprI32Const, 23,
      kExprThrow, except,
]).exportFunc();
let instance = builder.instantiate();
instance.exports.throw0();
