// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

let callee = builder.addFunction('callee', kSig_v_v).addBody([kExprNop]);

let body = [];
for (let i = 0; i < 600; i++) {
  body.push(kExprCallFunction, callee.index);
}

builder.addFunction('main', kSig_v_v).exportFunc().addBody(body);

let instance = builder.instantiate();
instance.exports.main();
