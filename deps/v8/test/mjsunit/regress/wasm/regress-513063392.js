// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.startRecGroup();
let $struct3 = builder.nextTypeIndex() + 1;
let $struct2 = builder.addStruct({fields: [], descriptor: $struct3});
assertEquals($struct3, builder.addStruct({fields: [], describes: $struct2}));
let $struct5 = builder.nextTypeIndex() + 1;
let $struct4 =
  builder.addStruct({fields: [], supertype: $struct2, descriptor: $struct5});
builder.addStruct({fields: [], supertype: $struct3, describes: $struct4});
let $array7 = builder.addArray(kWasmI32);
builder.endRecGroup();
let $sig31 = builder.addType(
  makeSig([wasmRefNullType($struct2)], [wasmRefNullType($struct2)]));
let $sig32 = builder.addType(
  makeSig([wasmRefNullType($struct5)], [kWasmI32, kWasmI32]));
let $func20 = builder.addFunction('main', kSig_v_v).exportFunc();
let $data0 = builder.addPassiveDataSegment(
  [116, 247, 220, 149, 175, 115, 104, 208, 32, 109, 25, 29, 172]);

// func $func20: [] -> []
$func20.addLocals(wasmRefNullType($struct2), 1)  // $var0
  .addLocals(kWasmI32, 1)  // $var1
  .addLocals(wasmRefNullType($struct2), 1)  // $var2
  .addBody([
    kGCPrefix, kExprStructNewDefault, $struct3,
    kGCPrefix, kExprStructNewDefaultDesc, $struct2,
    kExprLocalSet, 0,  // $var0
    kGCPrefix, kExprStructNewDefault, $struct3,
    kGCPrefix, kExprStructNewDefaultDesc, $struct2,
    kExprLoop, $sig31,
      kExprLocalSet, 2,  // $var2
      kExprLocalGet, 1,  // $var1
      kExprI32Const, 1,
      kExprI32Sub,
      kExprLocalTee, 1,  // $var1
      kExprIf, kWasmVoid,
        kExprRefNull, $struct5,
        kExprLocalGet, 0,  // $var0
        kGCPrefix, kExprRefCastNull, kStructRefCode,
        kGCPrefix, kExprRefCast, $struct3,
        kGCPrefix, kExprBrOnCastDescEq, 0b11, 1, kAnyRefCode, $struct2,
        kExprDrop,
        kExprLocalGet, 0,  // $var0
        kGCPrefix, kExprRefCastNull, $struct5,
        kExprBlock, $sig32,
          ...wasmI32Const(110754),
          ...wasmI32Const(246),
          kExprBr, 0,
        kExprEnd,
        kGCPrefix, kExprArrayNewData, $array7, $data0,
        kGCPrefix, kExprRefCast, kWasmExact, $struct3,
        kGCPrefix, kExprStructNewDefaultDesc, $struct2,
        kGCPrefix, kExprRefGetDesc, $struct2,
        kGCPrefix, kExprStructNewDefaultDesc, $struct2,
        kGCPrefix, kExprRefCastNull, $struct2,
        kExprBr, 1,
      kExprEnd,
      kExprRefNull, $struct2,
    kExprEnd,
    kExprLocalSet, 0,  // $var0
  ]);

const instance = builder.instantiate({});
assertTraps(kTrapIllegalCast, () => instance.exports.main());
