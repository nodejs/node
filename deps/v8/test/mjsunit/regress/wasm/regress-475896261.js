// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors --experimental-wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
builder.startRecGroup();
let $desc = builder.nextTypeIndex() + 1;
let $struct = builder.addStruct({
  descriptor: $desc,
  is_shared: true
});
builder.addStruct({
  describes: $struct,
  is_shared: true
});
builder.endRecGroup();

builder.addFunction("crash", kSig_v_v).exportFunc().addBody([
  kExprRefNull, kWasmSharedTypeForm, kNullRefCode,
  kGCPrefix, kExprRefGetDesc, $struct,
  kExprDrop,
]);
const instance = builder.instantiate();
assertTraps(kTrapNullDereference, () => instance.exports.crash());
