// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
builder.startRecGroup();
let $struct1 = builder.nextTypeIndex() + 1;
let $struct0 = builder.addStruct({fields: [], descriptor: $struct1});
/* $struct1 */ builder.addStruct({fields: [], final: true, describes: $struct0});
builder.endRecGroup();
builder.addFunction("main", kSig_v_v).exportFunc()
  .addLocals(wasmRefType($struct1), 1)
  .addBody([
    kGCPrefix, kExprStructNewDefault, $struct1,
    kExprLocalSet, 0,
    kExprLoop, kWasmVoid,
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kGCPrefix, kExprRefCastDesc, $struct0,
      kExprDrop,
      kExprBr, 0,
    kExprEnd,
    kExprUnreachable,
  ]);

const instance = builder.instantiate();
assertTraps(kTrapIllegalCast, () => instance.exports.main());
