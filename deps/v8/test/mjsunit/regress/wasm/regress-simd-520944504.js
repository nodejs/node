// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-wide-arithmetic --wasm-allow-mixed-eh-for-testing
// Flags: --no-liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let v0 = new WebAssembly.Global({ value: "i64", mutable: false });
let v1 = new WebAssembly.Tag({parameters: ["i32", "exnref", "exnref"]});
let v2 = new WebAssembly.Tag({parameters: ["f32", "i64", "i64", "externref", "eqref", "externref"]});

const builder = new WasmModuleBuilder();
builder.startRecGroup();
let $sig0 = builder.addType(makeSig([kWasmF32], [kWasmI64]));
builder.endRecGroup();
builder.startRecGroup();
let $sig1 = builder.addType(makeSig([], [wasmRefType(kWasmI31Ref)]));
builder.endRecGroup();
builder.startRecGroup();
let $sig2 = builder.addType(makeSig([kWasmI32, kWasmExnRef, kWasmExnRef], []));
builder.endRecGroup();
builder.startRecGroup();
let $sig3 = builder.addType(makeSig([kWasmF32, kWasmI64, kWasmI64, kWasmExternRef, kWasmEqRef, kWasmExternRef], []));
builder.endRecGroup();
builder.startRecGroup();
let $sig4 = builder.addType(makeSig([kWasmI32, kWasmExnRef, kWasmExnRef], []));
builder.endRecGroup();
builder.startRecGroup();
let $sig5 = builder.addType(makeSig([kWasmI32, kWasmFuncRef, kWasmI32, kWasmS128], []));
builder.endRecGroup();
let $sig6 = builder.addType(makeSig([kWasmI32, kWasmExnRef, kWasmExnRef], []));
let $sig7 = builder.addType(makeSig([kWasmF32, kWasmI64, kWasmI64, kWasmExternRef, kWasmEqRef, kWasmExternRef], []));
let $tag0 = builder.addImportedTag('imports', 'import_0_v1', $sig6);
let $tag1 = builder.addImportedTag('imports', 'import_1_v2', $sig7);
let $global0 = builder.addImportedGlobal('imports', 'import_2_v0', kWasmI64);
let w0 = builder.addFunction(undefined, $sig0);

// func $w0: [kWasmF32] -> [kWasmI64]
w0.addLocals(kWasmI32, 1)  // $var1
  .addLocals(kWasmExnRef, 1)  // $var2
  .addLocals(kWasmI32, 1)  // $var3
  .addLocals(wasmRefType(kWasmI31Ref), 1)  // $var4
  .addLocals(kWasmExnRef, 2)  // $var5 - $var6
  .addLocals(kWasmI32, 2)  // $var7 - $var8
  .addLocals(wasmRefType(kWasmI31Ref), 1)  // $var9
  .addLocals(kWasmExternRef, 1)  // $var10
  .addLocals(kWasmEqRef, 1)  // $var11
  .addLocals(kWasmExternRef, 1)  // $var12
  .addLocals(kWasmI64, 2)  // $var13 - $var14
  .addLocals(kWasmF32, 1)  // $var15
  .addLocals(kWasmFuncRef, 1)  // $var16
  .addLocals(kWasmS128, 1)  // $var17
  .addLocals(kWasmI32, 1)  // $var18
  .addLocals(kWasmS128, 1)  // $var19
  .addLocals(kWasmI32, 1)  // $var20
  .addLocals(kWasmFuncRef, 1)  // $var21
  .addLocals(kWasmI32, 1)  // $var22
  .addLocals(kWasmI64, 5)  // $var23 - $var27
  .addLocals(kWasmI32, 1)  // $var28
  .addLocals(wasmRefType(kWasmI31Ref), 1)  // $var29
  .addLocals(kWasmExnRef, 2)  // $var30 - $var31
  .addLocals(kWasmI32, 2)  // $var32 - $var33
  .addLocals(wasmRefType(kWasmI31Ref), 1)  // $var34
  .addLocals(kWasmI32, 1)  // $var35
  .addLocals(wasmRefType(kWasmI31Ref), 2)  // $var36 - $var37
  .addLocals(kWasmI64, 1)  // $var38
  .addBody([
    ...wasmI32Const(-266570842),
    kExprLocalSet, 1,  // $var1
    kExprTry, $sig1,
      kExprRefNull, kExnRefCode,
      kExprLocalSet, 2,  // $var2
      kExprLocalGet, 1,  // $var1
      kExprLocalGet, 2,  // $var2
      kExprLocalGet, 2,  // $var2
      kExprThrow, $tag0,
      ...wasmI32Const(1066147911),
      kExprLocalSet, 3,  // $var3
      kExprLocalGet, 3,  // $var3
      kGCPrefix, kExprRefI31,
      kExprLocalSet, 4,  // $var4
      kExprLocalGet, 4,  // $var4
    kExprCatch, $tag0,
      kExprLocalSet, 5,  // $var5
      kExprLocalSet, 6,  // $var6
      kExprLocalSet, 7,  // $var7
      ...wasmI32Const(-2),
      kExprLocalSet, 8,  // $var8
      kExprLocalGet, 8,  // $var8
      kGCPrefix, kExprRefI31,
      kExprLocalSet, 9,  // $var9
      kExprLocalGet, 9,  // $var9
    kExprCatch, $tag1,
      kExprLocalSet, 10,  // $var10
      kExprLocalSet, 11,  // $var11
      kExprLocalSet, 12,  // $var12
      kExprLocalSet, 13,  // $var13
      kExprLocalSet, 14,  // $var14
      kExprLocalSet, 15,  // $var15
      kExprRefNull, kFuncRefCode,
      kExprLocalSet, 16,  // $var16
      ...wasmS128Const([137, 15, 188, 54, 18, 48, 124, 85, 73, 45, 20, 52, 128, 145, 27, 225]),
      kExprLocalSet, 17,  // $var17
      kExprI32Const, 0,
      kExprLocalSet, 18,  // $var18
      kExprLocalGet, 18,  // $var18
      kExprLocalGet, 16,  // $var16
      kExprLocalGet, 1,  // $var1
      kExprLocalGet, 17,  // $var17
      kExprLoop, $sig5,
        kExprLocalSet, 19,  // $var19
        kExprLocalSet, 20,  // $var20
        kExprLocalSet, 21,  // $var21
        kExprLocalSet, 22,  // $var22
      kExprEnd,
      kExprGlobalGet, $global0,
      kExprLocalSet, 23,  // $var23
      kExprLocalGet, 23,  // $var23
      kExprLocalGet, 23,  // $var23
      kNumericPrefix, kExprI64MulWideU,
      kExprLocalSet, 24,  // $var24
      kExprLocalSet, 25,  // $var25
      kExprLocalGet, 24,  // $var24
      kExprLocalGet, 24,  // $var24
      kNumericPrefix, kExprI64MulWideU,
      kExprLocalSet, 26,  // $var26
      kExprLocalSet, 27,  // $var27
      kExprLocalGet, 0,  // $var0
      kExprLocalGet, 27,  // $var27
      kExprLocalGet, 14,  // $var14
      kExprLocalGet, 10,  // $var10
      kExprLocalGet, 11,  // $var11
      kExprLocalGet, 10,  // $var10
      kExprThrow, $tag1,
      ...wasmI32Const(1073741824),
      kExprLocalSet, 28,  // $var28
      kExprLocalGet, 28,  // $var28
      kGCPrefix, kExprRefI31,
      kExprLocalSet, 29,  // $var29
      kExprLocalGet, 29,  // $var29
    kExprCatch, $tag0,
      kExprLocalSet, 30,  // $var30
      kExprLocalSet, 31,  // $var31
      kExprLocalSet, 32,  // $var32
      ...wasmI32Const(1446812157),
      kExprLocalSet, 33,  // $var33
      kExprLocalGet, 33,  // $var33
      kGCPrefix, kExprRefI31,
      kExprLocalSet, 34,  // $var34
      kExprLocalGet, 34,  // $var34
    kExprCatchAll,
      ...wasmI32Const(-596636569),
      kExprLocalSet, 35,  // $var35
      kExprLocalGet, 35,  // $var35
      kGCPrefix, kExprRefI31,
      kExprLocalSet, 36,  // $var36
      kExprLocalGet, 36,  // $var36
    kExprEnd,
    kExprLocalSet, 37,  // $var37
    ...wasmI64Const(9223372036854775807n),
    kExprLocalSet, 38,  // $var38
    kExprLocalGet, 38,  // $var38
  ]);

builder.addExport('w0', w0.index);

const instance = builder.instantiate({ imports: {
    import_0_v1: v1,
    import_1_v2: v2,
    import_2_v0: v0,
} });

instance.exports.w0();
