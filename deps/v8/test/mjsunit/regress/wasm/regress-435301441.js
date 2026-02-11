// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.startRecGroup();
let $desc1 = builder.nextTypeIndex() + 1;
let $struct0 = builder.addStruct({descriptor: $desc1});
/* $desc1 */ builder.addStruct({describes: $struct0});
builder.endRecGroup();

builder.addFunction("main", kSig_v_v).exportFunc().addBody([
  kExprRefNull, kWasmExact, $struct0,
  kGCPrefix, kExprRefGetDesc, $struct0,
  kExprRefNull, kWasmExact, $struct0,
  kGCPrefix, kExprRefGetDesc, $struct0,
  kGCPrefix, kExprRefCastDescNull, $struct0,
  kExprRefNull, kWasmExact, $struct0,
  kGCPrefix, kExprRefGetDesc, $struct0,
  kGCPrefix, kExprStructNewDefaultDesc, $struct0,
  kExprDrop,
  kExprDrop,
]);

let instance = builder.instantiate();
assertTraps(kTrapNullDereference, () => instance.exports.main());
