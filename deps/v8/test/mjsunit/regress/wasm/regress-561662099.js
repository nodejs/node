// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors --no-liftoff --no-wasm-inlining

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
builder.startRecGroup();
let $struct1 = builder.nextTypeIndex() + 1;
let $struct0 = builder.addStruct({fields: [], final: true, descriptor: $struct1});
/* $struct1 */ builder.addStruct({fields: [], final: true, describes: $struct0});
builder.endRecGroup();

let $tag = builder.addTag(makeSig([wasmRefType($struct1)], []));

builder.addFunction("thrower", kSig_v_v)
  .addBody([
    kGCPrefix, kExprStructNewDefault, $struct1,
    kExprThrow, $tag,
  ]);

builder.addFunction("main", kSig_v_v).exportFunc()
  .addLocals(wasmRefType($struct1), 1)
  .addBody([
    kExprBlock, kWasmRef, $struct1,
      kExprTryTable, kWasmVoid, 1,
      kCatchNoRef, $tag, 0,
        kExprCallFunction, 0,
      kExprEnd,
      kExprUnreachable,
    kExprEnd,
    kExprLocalSet, 0,

    kExprLoop, kWasmVoid,
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kGCPrefix, kExprRefCastDescEq, $struct0,
      kExprDrop,
      kExprBr, 0,
    kExprEnd,
  ]);

const instance = builder.instantiate();
assertThrows(() => instance.exports.main(), WebAssembly.RuntimeError, /illegal cast/);
