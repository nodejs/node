// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
let params = [];
let size = 139;
for (let i = 0; i < size; i++) {
  params.push(kWasmI64);
}
let fn = builder.addFunction("callee", makeSig(params, [])).addBody([]);
let body = [];
for (let i = 0; i < size; i++) {
  body.push(kExprI64Const, i);
  body.push(kExprI64Const, i);
}
body.push(kExprReturnCall, fn.index);
builder.addFunction("main", kSig_v_v).exportFunc().addBody(body);
builder.instantiate().exports.main();
