// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.startRecGroup();
let $struct1 = builder.nextTypeIndex() + 1;
let $struct0 = builder.addStruct({fields: [], final: true, descriptor: $struct1});
/* $struct1 */ builder.addStruct({fields: [], final: true, describes: $struct0});
let $struct2 = builder.nextTypeIndex();
let $struct3 = builder.nextTypeIndex() + 1;
/* $struct2 */ builder.addStruct({fields: [], descriptor: $struct3});
/* $struct3 */ builder.addStruct({fields: [], describes: $struct2});
builder.endRecGroup();

let w8 = builder.addFunction(undefined, kSig_v_v).exportAs('w8')
  .addBody([
    kExprBlock, kWasmRefNull, $struct0,
      kGCPrefix, kExprStructNewDefault, $struct3,
      kGCPrefix, kExprStructNewDefault, $struct1,
      kGCPrefix, kExprBrOnCastDescEq,
          0b11 /* nullable -> nullable */, 0, kAnyRefCode, kWasmExact, $struct0,
      kExprDrop,
      kExprRefNull, kWasmExact, $struct0,
    kExprEnd,
    kExprDrop,
  ]);

const instance = builder.instantiate();
instance.exports.w8();
%WasmTierUpFunction(instance.exports.w8);
instance.exports.w8();
