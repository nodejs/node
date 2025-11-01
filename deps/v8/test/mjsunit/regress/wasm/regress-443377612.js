// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.startRecGroup();
let $desc = builder.nextTypeIndex() + 1;
let $struct = builder.addStruct({descriptor: $desc});
/* $desc */ builder.addStruct({describes: $struct});
builder.endRecGroup();

builder.addFunction("main", makeSig([], [kWasmAnyRef])).exportFunc().addBody([
  kExprRefNull, kNullRefCode,
  kGCPrefix, kExprRefGetDesc, $struct,
]);

let instance = builder.instantiate();

assertTraps(kTrapNullDereference, () => instance.exports.main());
