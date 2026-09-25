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

builder.addFunction("main", kSig_v_v).exportFunc()
  .addLocals(wasmRefNullType($desc1).exact(), 1)
  .addBody([
    kExprLoop, kWasmVoid,
      kExprBlock, kWasmVoid,
        kExprLocalGet, 0,
        kGCPrefix, kExprStructNewDefaultDesc, $struct0,
        kExprDrop,
        kExprI32Const, 0,
        kExprIf, kWasmVoid,
          kExprLoop, kWasmVoid,
            kExprI32Const, 0,
            kExprBrTable, 1, 2, 0,
          kExprEnd,  // loop
        kExprEnd,  // if
      kExprEnd,  // block
      kExprI32Const, 0,
      kGCPrefix, kExprRefI31,
      kGCPrefix, kExprI31GetU,
      kExprBrIf, 0,
    kExprEnd,  // loop
    kExprLocalGet, 0,
    kGCPrefix, kExprStructNewDefaultDesc, $struct0,
    kExprDrop,
  ]);

let instance = builder.instantiate();
assertTraps(kTrapNullDereference, () => instance.exports.main());
