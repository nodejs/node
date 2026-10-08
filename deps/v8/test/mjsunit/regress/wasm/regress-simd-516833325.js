// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
let s128s = builder.addType(makeSig(Array(1000).fill(kWasmS128), []));
let callee = builder.addFunction('h', s128s).addBody([]);
let caller = builder.addFunction('f', kSig_v_i);
let body = [kExprLocalGet, 0, kExprIf, kWasmVoid];
for (let i = 0; i < 1000; i++) {
  body.push(kExprI32Const, 0, kSimdPrefix, kExprI32x4Splat);
}
body.push(kExprReturnCall, callee.index, kExprEnd);
caller.addBody(body).exportFunc();
let instance = builder.instantiate();
instance.exports.f();
