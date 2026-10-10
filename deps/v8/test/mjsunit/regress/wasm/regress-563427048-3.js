// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.startRecGroup();
let $struct1 = builder.nextTypeIndex() + 1;
let $struct0 = builder.addStruct({fields: [], descriptor: $struct1});
let $struct2 = builder.nextTypeIndex() + 1;
/* $struct1 */ builder.addStruct({fields: [], descriptor: $struct2, describes: $struct0});
let $struct3 = builder.nextTypeIndex() + 1;
/* $struct2 */ builder.addStruct({fields: [], descriptor: $struct3, describes: $struct1});
/* $struct3 */ builder.addStruct({fields: [], describes: $struct2});
builder.endRecGroup();

builder.addFunction('func_80_invoker', kSig_v_v).exportFunc()
  .addBody([
    kGCPrefix, kExprStructNewDefault, $struct3,
    kGCPrefix, kExprStructNewDefaultDesc, $struct2,
    kGCPrefix, kExprStructNewDefaultDesc, $struct1,
    kGCPrefix, kExprRefGetDesc, $struct1,
    kGCPrefix, kExprStructNewDefault, $struct3,
    kGCPrefix, kExprRefCastDescEq, kWasmExact, $struct2,
    kExprUnreachable,
  ]);

const instance = builder.instantiate({});
assertThrows(() => instance.exports.func_80_invoker(), WebAssembly.RuntimeError,
             /illegal cast/);
