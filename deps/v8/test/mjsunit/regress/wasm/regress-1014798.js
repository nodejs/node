// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('main', kSig_i_iii)
    .addLocals({f32_count: 4})
    .addLocals({i64_count: 1})
    .addLocals({f32_count: 2})
    .addBodyWithEnd([
      kExprI64Const, 0,
      kExprLocalGet, 3,
      kExprI64SConvertF32,
      kExprI64Ne,
      kExprEnd,  // @17
    ]).exportFunc();
const instance = builder.instantiate();
assertEquals(0, instance.exports.main(1, 2, 3));
