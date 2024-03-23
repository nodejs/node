// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-stringref

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestRefTest() {
  var builder = new WasmModuleBuilder();
  builder.startRecGroup();
  let structSuper = builder.addStruct([makeField(kWasmI32, true)]);
  let structSub = builder.addStruct([makeField(kWasmI32, true)], structSuper);
  let array = builder.addArray(kWasmI32);
  builder.endRecGroup();

  let fct =
  builder.addFunction('createStructSuper',
                      makeSig([kWasmI32], [kWasmExternRef]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStructNew, structSuper,
      kGCPrefix, kExprExternConvertAny,
    ]).exportFunc();
  builder.addFunction('createStructSub', makeSig([kWasmI32], [kWasmExternRef]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStructNew, structSub,
      kGCPrefix, kExprExternConvertAny,
    ]).exportFunc();
  builder.addFunction('createArray', makeSig([kWasmI32], [kWasmExternRef]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprArrayNewFixed, array, 1,
      kGCPrefix, kExprExternConvertAny,
    ]).exportFunc();
  builder.addFunction('createFuncRef', makeSig([], [kWasmFuncRef]))
    .addBody([
      kExprRefFunc, fct.index,
    ]).exportFunc();

  [
    ["StructSuper", structSuper],
    ["StructSub", structSub],
    ["Array", array],
    ["I31", kI31RefCode],
    ["AnyArray", kArrayRefCode],
    ["Struct", kStructRefCode],
    ["Eq", kEqRefCode],
    ["String", kStringRefCode],
    // 'ref.test any' is semantically the same as '!ref.is_null' here.
    ["Any", kAnyRefCode],
    ["None", kNullRefCode]
  ].forEach(([typeName, typeCode]) => {
    builder.addFunction(`refTest${typeName}`,
                        makeSig([kWasmExternRef], [kWasmI32, kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefTest, typeCode,
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefTestNull, typeCode,
    ]).exportFunc();

    builder.addFunction(`refCast${typeName}`,
                        makeSig([kWasmExternRef], [kWasmExternRef]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, typeCode,
      kGCPrefix, kExprExternConvertAny,
    ]).exportFunc();

    builder.addFunction(`refCastNull${typeName}`,
                        makeSig([kWasmExternRef], [kWasmExternRef]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCastNull, typeCode,
      kGCPrefix, kExprExternConvertAny,
    ]).exportFunc();

    builder.addFunction(`brOnCastGeneric${typeName}`,
                        makeSig([kWasmExternRef], [kWasmI32]))
    .addBody([
      kExprBlock, kWasmRef, typeCode,
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprBrOnCastGeneric, 0b01, 0, kAnyRefCode, typeCode,
        kExprI32Const, 0,
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 1,
      kExprReturn,
    ]).exportFunc();
    builder.addFunction(`brOnCastGenericNull${typeName}`,
                        makeSig([kWasmExternRef], [kWasmI32]))
    .addBody([
      kExprBlock, kWasmRefNull, typeCode,
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprBrOnCastGeneric, 0b11, 0, kAnyRefCode, typeCode,
        kExprI32Const, 0,
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 1,
      kExprReturn,
    ]).exportFunc();
    builder.addFunction(`brOnCastGenericFail${typeName}`,
                        makeSig([kWasmExternRef], [kWasmI32]))
    .addBody([
      kExprBlock, kAnyRefCode,
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprBrOnCastFailGeneric, 0b01, 0, kAnyRefCode, typeCode,
        kExprI32Const, 0,
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 1,
      kExprReturn,
    ]).exportFunc();
    builder.addFunction(`brOnCastGenericFailNull${typeName}`,
                        makeSig([kWasmExternRef], [kWasmI32]))
    .addBody([
      kExprBlock, kAnyRefCode,
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprBrOnCastFailGeneric, 0b11, 0, kAnyRefCode, typeCode,
        kExprI32Const, 0,
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 1,
      kExprReturn,
    ]).exportFunc();
  });

  var instance = builder.instantiate();
  let wasm = instance.exports;
  // result: [ref.test, ref.test null]
  assertEquals([0, 1], wasm.refTestStructSuper(null));
  assertEquals([0, 0], wasm.refTestStructSuper(undefined));
  assertEquals([1, 1], wasm.refTestStructSuper(wasm.createStructSuper()));
  assertEquals([1, 1], wasm.refTestStructSuper(wasm.createStructSub()));
  assertEquals([0, 0], wasm.refTestStructSuper(wasm.createArray()));
  assertEquals([0, 0], wasm.refTestStructSuper(wasm.createFuncRef()));
  assertEquals([0, 0], wasm.refTestStructSuper(1));
  assertEquals([0, 0], wasm.refTestStructSuper({'JavaScript': 'Object'}));
  assertEquals([0, 0], wasm.refTestStructSuper('string'));

  assertEquals([0, 1], wasm.refTestStructSub(null));
  assertEquals([0, 0], wasm.refTestStructSub(undefined));
  assertEquals([0, 0], wasm.refTestStructSub(wasm.createStructSuper()));
  assertEquals([1, 1], wasm.refTestStructSub(wasm.createStructSub()));
  assertEquals([0, 0], wasm.refTestStructSub(wasm.createArray()));
  assertEquals([0, 0], wasm.refTestStructSub(wasm.createFuncRef()));
  assertEquals([0, 0], wasm.refTestStructSub(1));
  assertEquals([0, 0], wasm.refTestStructSub({'JavaScript': 'Object'}));
  assertEquals([0, 0], wasm.refTestStructSub('string'));

  assertEquals([0, 1], wasm.refTestArray(null));
  assertEquals([0, 0], wasm.refTestArray(undefined));
  assertEquals([0, 0], wasm.refTestArray(wasm.createStructSuper()));
  assertEquals([0, 0], wasm.refTestArray(wasm.createStructSub()));
  assertEquals([1, 1], wasm.refTestArray(wasm.createArray()));
  assertEquals([0, 0], wasm.refTestArray(wasm.createFuncRef()));
  assertEquals([0, 0], wasm.refTestArray(1));
  assertEquals([0, 0], wasm.refTestArray({'JavaScript': 'Object'}));
  assertEquals([0, 0], wasm.refTestArray('string'));

  assertEquals([0, 1], wasm.refTestI31(null));
  assertEquals([0, 0], wasm.refTestI31(undefined));
  assertEquals([0, 0], wasm.refTestI31(wasm.createStructSuper()));
  assertEquals([0, 0], wasm.refTestI31(wasm.createStructSub()));
  assertEquals([0, 0], wasm.refTestI31(wasm.createArray()));
  assertEquals([0, 0], wasm.refTestI31(wasm.createFuncRef()));
  assertEquals([1, 1], wasm.refTestI31(1));
  assertEquals([0, 0], wasm.refTestI31({'JavaScript': 'Object'}));
  assertEquals([0, 0], wasm.refTestI31('string'));

  assertEquals([0, 1], wasm.refTestAnyArray(null));
  assertEquals([0, 0], wasm.refTestAnyArray(undefined));
  assertEquals([0, 0], wasm.refTestAnyArray(wasm.createStructSuper()));
  assertEquals([0, 0], wasm.refTestAnyArray(wasm.createStructSub()));
  assertEquals([1, 1], wasm.refTestAnyArray(wasm.createArray()));
  assertEquals([0, 0], wasm.refTestAnyArray(wasm.createFuncRef()));
  assertEquals([0, 0], wasm.refTestAnyArray(1));
  assertEquals([0, 0], wasm.refTestAnyArray({'JavaScript': 'Object'}));
  assertEquals([0, 0], wasm.refTestAnyArray('string'));

  assertEquals([0, 1], wasm.refTestStruct(null));
  assertEquals([0, 0], wasm.refTestStruct(undefined));
  assertEquals([1, 1], wasm.refTestStruct(wasm.createStructSuper()));
  assertEquals([1, 1], wasm.refTestStruct(wasm.createStructSub()));
  assertEquals([0, 0], wasm.refTestStruct(wasm.createArray()));
  assertEquals([0, 0], wasm.refTestStruct(wasm.createFuncRef()));
  assertEquals([0, 0], wasm.refTestStruct(1));
  assertEquals([0, 0], wasm.refTestStruct({'JavaScript': 'Object'}));
  assertEquals([0, 0], wasm.refTestStruct('string'));

  assertEquals([0, 1], wasm.refTestString(null));
  assertEquals([0, 0], wasm.refTestString(undefined));
  assertEquals([0, 0], wasm.refTestString(wasm.createStructSuper()));
  assertEquals([0, 0], wasm.refTestString(wasm.createStructSub()));
  assertEquals([0, 0], wasm.refTestString(wasm.createArray()));
  assertEquals([0, 0], wasm.refTestString(wasm.createFuncRef()));
  assertEquals([0, 0], wasm.refTestString(1));
  assertEquals([0, 0], wasm.refTestString({'JavaScript': 'Object'}));
  assertEquals([1, 1], wasm.refTestString('string'));

  assertEquals([0, 1], wasm.refTestEq(null));
  assertEquals([0, 0], wasm.refTestEq(undefined));
  assertEquals([1, 1], wasm.refTestEq(wasm.createStructSuper()));
  assertEquals([1, 1], wasm.refTestEq(wasm.createStructSub()));
  assertEquals([1, 1], wasm.refTestEq(wasm.createArray()));
  assertEquals([0, 0], wasm.refTestEq(wasm.createFuncRef()));
  assertEquals([1, 1], wasm.refTestEq(1)); // ref.i31
  assertEquals([0, 0], wasm.refTestEq({'JavaScript': 'Object'}));
  assertEquals([0, 0], wasm.refTestEq('string'));

  assertEquals([0, 1], wasm.refTestAny(null));
  assertEquals([1, 1], wasm.refTestAny(undefined));
  assertEquals([1, 1], wasm.refTestAny(wasm.createStructSuper()));
  assertEquals([1, 1], wasm.refTestAny(wasm.createStructSub()));
  assertEquals([1, 1], wasm.refTestAny(wasm.createArray()));
  assertEquals([1, 1], wasm.refTestAny(wasm.createFuncRef()));
  assertEquals([1, 1], wasm.refTestAny(1)); // ref.i31
  assertEquals([1, 1], wasm.refTestAny({'JavaScript': 'Object'}));
  assertEquals([1, 1], wasm.refTestAny('string'));

  assertEquals([0, 1], wasm.refTestNone(null));
  assertEquals([0, 0], wasm.refTestNone(undefined));
  assertEquals([0, 0], wasm.refTestNone(wasm.createStructSuper()));
  assertEquals([0, 0], wasm.refTestNone(wasm.createStructSub()));
  assertEquals([0, 0], wasm.refTestNone(wasm.createArray()));
  assertEquals([0, 0], wasm.refTestNone(wasm.createFuncRef()));
  assertEquals([0, 0], wasm.refTestNone(1)); // ref.i31
  assertEquals([0, 0], wasm.refTestNone({'JavaScript': 'Object'}));
  assertEquals([0, 0], wasm.refTestNone('string'));

  // ref.cast
  let structSuperObj = wasm.createStructSuper();
  let structSubObj = wasm.createStructSub();
  let arrayObj = wasm.createArray();
  let jsObj = {'JavaScript': 'Object'};
  let strObj = 'string';
  let funcObj = wasm.createFuncRef();

  assertTraps(kTrapIllegalCast, () => wasm.refCastStructSuper(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStructSuper(undefined));
  assertSame(structSuperObj, wasm.refCastStructSuper(structSuperObj));
  assertSame(structSubObj, wasm.refCastStructSuper(structSubObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStructSuper(arrayObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStructSuper(funcObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStructSuper(1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStructSuper(jsObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStructSuper(strObj));

  assertTraps(kTrapIllegalCast, () => wasm.refCastStructSub(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStructSub(undefined));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStructSub(structSuperObj));
  assertSame(structSubObj, wasm.refCastStructSub(structSubObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStructSub(arrayObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStructSub(funcObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStructSub(1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStructSub(jsObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStructSub(strObj));

  assertTraps(kTrapIllegalCast, () => wasm.refCastArray(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastArray(undefined));
  assertTraps(kTrapIllegalCast, () => wasm.refCastArray(structSuperObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastArray(structSubObj));
  assertSame(arrayObj, wasm.refCastArray(arrayObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastArray(funcObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastArray(1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastArray(jsObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastArray(strObj));

  assertTraps(kTrapIllegalCast, () => wasm.refCastI31(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastI31(undefined));
  assertTraps(kTrapIllegalCast, () => wasm.refCastI31(structSuperObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastI31(structSubObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastI31(arrayObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastI31(funcObj));
  assertEquals(1, wasm.refCastI31(1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastI31(jsObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastI31(strObj));

  assertTraps(kTrapIllegalCast, () => wasm.refCastAnyArray(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastAnyArray(undefined));
  assertTraps(kTrapIllegalCast, () => wasm.refCastAnyArray(structSuperObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastAnyArray(structSubObj));
  assertSame(arrayObj, wasm.refCastAnyArray(arrayObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastAnyArray(funcObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastAnyArray(1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastAnyArray(jsObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastAnyArray(strObj));

  assertTraps(kTrapIllegalCast, () => wasm.refCastStruct(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStruct(undefined));
  assertSame(structSuperObj, wasm.refCastStruct(structSuperObj));
  assertSame(structSubObj, wasm.refCastStruct(structSubObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStruct(arrayObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStruct(funcObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStruct(1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStruct(jsObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStruct(strObj));

  assertTraps(kTrapIllegalCast, () => wasm.refCastString(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastString(undefined));
  assertTraps(kTrapIllegalCast, () => wasm.refCastString(structSuperObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastString(structSubObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastString(arrayObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastString(funcObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastString(1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastString(jsObj));
  assertSame(strObj, wasm.refCastString(strObj));

  assertTraps(kTrapIllegalCast, () => wasm.refCastEq(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastEq(undefined));
  assertSame(structSuperObj, wasm.refCastEq(structSuperObj));
  assertSame(structSubObj, wasm.refCastEq(structSubObj));
  assertSame(arrayObj, wasm.refCastEq(arrayObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastEq(funcObj));
  assertEquals(1, wasm.refCastEq(1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastEq(jsObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastEq(strObj));

  assertTraps(kTrapIllegalCast, () => wasm.refCastAny(null));
  assertSame(undefined, wasm.refCastAny(undefined));
  assertSame(structSuperObj, wasm.refCastAny(structSuperObj));
  assertSame(structSubObj, wasm.refCastAny(structSubObj));
  assertSame(arrayObj, wasm.refCastAny(arrayObj));
  assertSame(funcObj, wasm.refCastAny(funcObj));
  assertEquals(1, wasm.refCastAny(1));
  assertSame(jsObj, wasm.refCastAny(jsObj));
  assertSame(strObj, wasm.refCastAny(strObj));

  assertTraps(kTrapIllegalCast, () => wasm.refCastNone(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNone(undefined));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNone(structSuperObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNone(structSubObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNone(arrayObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNone(funcObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNone(1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNone(jsObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNone(strObj));

  // ref.cast null
  assertSame(null, wasm.refCastNullStructSuper(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStructSuper(undefined));
  assertSame(structSuperObj, wasm.refCastNullStructSuper(structSuperObj));
  assertSame(structSubObj, wasm.refCastNullStructSuper(structSubObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStructSuper(arrayObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStructSuper(funcObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStructSuper(1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStructSuper(jsObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStructSuper(strObj));

  assertSame(null, wasm.refCastNullStructSub(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStructSub(undefined));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStructSub(structSuperObj));
  assertSame(structSubObj, wasm.refCastNullStructSub(structSubObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStructSub(arrayObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStructSub(funcObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStructSub(1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStructSub(jsObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStructSub(strObj));

  assertSame(null, wasm.refCastNullArray(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullArray(undefined));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullArray(structSuperObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullArray(structSubObj));
  assertSame(arrayObj, wasm.refCastNullArray(arrayObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullArray(funcObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullArray(1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullArray(jsObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullArray(strObj));

  assertSame(null, wasm.refCastNullI31(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullI31(undefined));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullI31(structSuperObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullI31(structSubObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullI31(arrayObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullI31(funcObj));
  assertEquals(1, wasm.refCastNullI31(1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullI31(jsObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullI31(strObj));

  assertSame(null, wasm.refCastNullAnyArray(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullAnyArray(undefined));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullAnyArray(structSuperObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullAnyArray(structSubObj));
  assertSame(arrayObj, wasm.refCastNullAnyArray(arrayObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullAnyArray(funcObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullAnyArray(1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullAnyArray(jsObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullAnyArray(strObj));

  assertSame(null, wasm.refCastNullStruct(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStruct(undefined));
  assertSame(structSuperObj, wasm.refCastNullStruct(structSuperObj));
  assertSame(structSubObj, wasm.refCastNullStruct(structSubObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStruct(arrayObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStruct(funcObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStruct(1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStruct(jsObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStruct(strObj));

  assertSame(null, wasm.refCastNullString(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullString(undefined));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullString(structSuperObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullString(structSubObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullString(arrayObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullString(funcObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullString(1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullString(jsObj));
  assertSame(strObj, wasm.refCastNullString(strObj));

  assertSame(null, wasm.refCastNullEq(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullEq(undefined));
  assertSame(structSuperObj, wasm.refCastNullEq(structSuperObj));
  assertSame(structSubObj, wasm.refCastNullEq(structSubObj));
  assertSame(arrayObj, wasm.refCastNullEq(arrayObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullEq(funcObj));
  assertEquals(1, wasm.refCastNullEq(1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullEq(jsObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullEq(strObj));

  assertSame(null, wasm.refCastNullAny(null));
  assertSame(undefined, wasm.refCastNullAny(undefined));
  assertSame(structSuperObj, wasm.refCastNullAny(structSuperObj));
  assertSame(structSubObj, wasm.refCastNullAny(structSubObj));
  assertSame(arrayObj, wasm.refCastNullAny(arrayObj));
  assertSame(funcObj, wasm.refCastNullAny(funcObj));
  assertEquals(1, wasm.refCastNullAny(1));
  assertSame(jsObj, wasm.refCastNullAny(jsObj));
  assertSame(strObj, wasm.refCastNullAny(strObj));

  assertSame(null, wasm.refCastNullNone(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullNone(undefined));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullNone(structSuperObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullNone(structSubObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullNone(arrayObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullNone(funcObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullNone(1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullNone(jsObj));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullNone(strObj));

  // br_on_cast
  assertEquals(0, wasm.brOnCastGenericStructSuper(null));
  assertEquals(0, wasm.brOnCastGenericStructSuper(undefined));
  assertEquals(1, wasm.brOnCastGenericStructSuper(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericStructSuper(structSubObj));
  assertEquals(0, wasm.brOnCastGenericStructSuper(arrayObj));
  assertEquals(0, wasm.brOnCastGenericStructSuper(funcObj));
  assertEquals(0, wasm.brOnCastGenericStructSuper(1));
  assertEquals(0, wasm.brOnCastGenericStructSuper(jsObj));
  assertEquals(0, wasm.brOnCastGenericStructSuper(strObj));

  assertEquals(0, wasm.brOnCastGenericStructSub(null));
  assertEquals(0, wasm.brOnCastGenericStructSub(undefined));
  assertEquals(0, wasm.brOnCastGenericStructSub(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericStructSub(structSubObj));
  assertEquals(0, wasm.brOnCastGenericStructSub(arrayObj));
  assertEquals(0, wasm.brOnCastGenericStructSub(funcObj));
  assertEquals(0, wasm.brOnCastGenericStructSub(1));
  assertEquals(0, wasm.brOnCastGenericStructSub(jsObj));
  assertEquals(0, wasm.brOnCastGenericStructSub(strObj));

  assertEquals(0, wasm.brOnCastGenericArray(null));
  assertEquals(0, wasm.brOnCastGenericArray(undefined));
  assertEquals(0, wasm.brOnCastGenericArray(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericArray(structSubObj));
  assertEquals(1, wasm.brOnCastGenericArray(arrayObj));
  assertEquals(0, wasm.brOnCastGenericArray(funcObj));
  assertEquals(0, wasm.brOnCastGenericArray(1));
  assertEquals(0, wasm.brOnCastGenericArray(jsObj));
  assertEquals(0, wasm.brOnCastGenericArray(strObj));

  assertEquals(0, wasm.brOnCastGenericI31(null));
  assertEquals(0, wasm.brOnCastGenericI31(undefined));
  assertEquals(0, wasm.brOnCastGenericI31(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericI31(structSubObj));
  assertEquals(0, wasm.brOnCastGenericI31(arrayObj));
  assertEquals(0, wasm.brOnCastGenericI31(funcObj));
  assertEquals(1, wasm.brOnCastGenericI31(1));
  assertEquals(0, wasm.brOnCastGenericI31(jsObj));
  assertEquals(0, wasm.brOnCastGenericI31(strObj));

  assertEquals(0, wasm.brOnCastGenericAnyArray(null));
  assertEquals(0, wasm.brOnCastGenericAnyArray(undefined));
  assertEquals(0, wasm.brOnCastGenericAnyArray(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericAnyArray(structSubObj));
  assertEquals(1, wasm.brOnCastGenericAnyArray(arrayObj));
  assertEquals(0, wasm.brOnCastGenericAnyArray(funcObj));
  assertEquals(0, wasm.brOnCastGenericAnyArray(1));
  assertEquals(0, wasm.brOnCastGenericAnyArray(jsObj));
  assertEquals(0, wasm.brOnCastGenericAnyArray(strObj));

  assertEquals(0, wasm.brOnCastGenericStruct(null));
  assertEquals(0, wasm.brOnCastGenericStruct(undefined));
  assertEquals(1, wasm.brOnCastGenericStruct(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericStruct(structSubObj));
  assertEquals(0, wasm.brOnCastGenericStruct(arrayObj));
  assertEquals(0, wasm.brOnCastGenericStruct(funcObj));
  assertEquals(0, wasm.brOnCastGenericStruct(1));
  assertEquals(0, wasm.brOnCastGenericStruct(jsObj));
  assertEquals(0, wasm.brOnCastGenericStruct(strObj));

  assertEquals(0, wasm.brOnCastGenericEq(null));
  assertEquals(0, wasm.brOnCastGenericEq(undefined));
  assertEquals(1, wasm.brOnCastGenericEq(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericEq(structSubObj));
  assertEquals(1, wasm.brOnCastGenericEq(arrayObj));
  assertEquals(0, wasm.brOnCastGenericEq(funcObj));
  assertEquals(1, wasm.brOnCastGenericEq(1));
  assertEquals(0, wasm.brOnCastGenericEq(jsObj));
  assertEquals(0, wasm.brOnCastGenericEq(strObj));

  assertEquals(0, wasm.brOnCastGenericString(null));
  assertEquals(0, wasm.brOnCastGenericString(undefined));
  assertEquals(0, wasm.brOnCastGenericString(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericString(structSubObj));
  assertEquals(0, wasm.brOnCastGenericString(arrayObj));
  assertEquals(0, wasm.brOnCastGenericString(funcObj));
  assertEquals(0, wasm.brOnCastGenericString(1));
  assertEquals(0, wasm.brOnCastGenericString(jsObj));
  assertEquals(1, wasm.brOnCastGenericString(strObj));

  assertEquals(0, wasm.brOnCastGenericAny(null));
  assertEquals(1, wasm.brOnCastGenericAny(undefined));
  assertEquals(1, wasm.brOnCastGenericAny(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericAny(structSubObj));
  assertEquals(1, wasm.brOnCastGenericAny(arrayObj));
  assertEquals(1, wasm.brOnCastGenericAny(funcObj));
  assertEquals(1, wasm.brOnCastGenericAny(1));
  assertEquals(1, wasm.brOnCastGenericAny(jsObj));
  assertEquals(1, wasm.brOnCastGenericAny(strObj));

  assertEquals(0, wasm.brOnCastGenericNone(null));
  assertEquals(0, wasm.brOnCastGenericNone(undefined));
  assertEquals(0, wasm.brOnCastGenericNone(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericNone(structSubObj));
  assertEquals(0, wasm.brOnCastGenericNone(arrayObj));
  assertEquals(0, wasm.brOnCastGenericNone(funcObj));
  assertEquals(0, wasm.brOnCastGenericNone(1));
  assertEquals(0, wasm.brOnCastGenericNone(jsObj));
  assertEquals(0, wasm.brOnCastGenericNone(strObj));

  // br_on_cast null
  assertEquals(1, wasm.brOnCastGenericNullStructSuper(null));
  assertEquals(0, wasm.brOnCastGenericNullStructSuper(undefined));
  assertEquals(1, wasm.brOnCastGenericNullStructSuper(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericNullStructSuper(structSubObj));
  assertEquals(0, wasm.brOnCastGenericNullStructSuper(arrayObj));
  assertEquals(0, wasm.brOnCastGenericNullStructSuper(funcObj));
  assertEquals(0, wasm.brOnCastGenericNullStructSuper(1));
  assertEquals(0, wasm.brOnCastGenericNullStructSuper(jsObj));
  assertEquals(0, wasm.brOnCastGenericNullStructSuper(strObj));

  assertEquals(1, wasm.brOnCastGenericNullStructSub(null));
  assertEquals(0, wasm.brOnCastGenericNullStructSub(undefined));
  assertEquals(0, wasm.brOnCastGenericNullStructSub(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericNullStructSub(structSubObj));
  assertEquals(0, wasm.brOnCastGenericNullStructSub(arrayObj));
  assertEquals(0, wasm.brOnCastGenericNullStructSub(funcObj));
  assertEquals(0, wasm.brOnCastGenericNullStructSub(1));
  assertEquals(0, wasm.brOnCastGenericNullStructSub(jsObj));
  assertEquals(0, wasm.brOnCastGenericNullStructSub(strObj));

  assertEquals(1, wasm.brOnCastGenericNullArray(null));
  assertEquals(0, wasm.brOnCastGenericNullArray(undefined));
  assertEquals(0, wasm.brOnCastGenericNullArray(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericNullArray(structSubObj));
  assertEquals(1, wasm.brOnCastGenericNullArray(arrayObj));
  assertEquals(0, wasm.brOnCastGenericNullArray(funcObj));
  assertEquals(0, wasm.brOnCastGenericNullArray(1));
  assertEquals(0, wasm.brOnCastGenericNullArray(jsObj));
  assertEquals(0, wasm.brOnCastGenericNullArray(strObj));

  assertEquals(1, wasm.brOnCastGenericNullI31(null));
  assertEquals(0, wasm.brOnCastGenericNullI31(undefined));
  assertEquals(0, wasm.brOnCastGenericNullI31(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericNullI31(structSubObj));
  assertEquals(0, wasm.brOnCastGenericNullI31(arrayObj));
  assertEquals(0, wasm.brOnCastGenericNullI31(funcObj));
  assertEquals(1, wasm.brOnCastGenericNullI31(1));
  assertEquals(0, wasm.brOnCastGenericNullI31(jsObj));
  assertEquals(0, wasm.brOnCastGenericNullI31(strObj));

  assertEquals(1, wasm.brOnCastGenericNullAnyArray(null));
  assertEquals(0, wasm.brOnCastGenericNullAnyArray(undefined));
  assertEquals(0, wasm.brOnCastGenericNullAnyArray(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericNullAnyArray(structSubObj));
  assertEquals(1, wasm.brOnCastGenericNullAnyArray(arrayObj));
  assertEquals(0, wasm.brOnCastGenericNullAnyArray(funcObj));
  assertEquals(0, wasm.brOnCastGenericNullAnyArray(1));
  assertEquals(0, wasm.brOnCastGenericNullAnyArray(jsObj));
  assertEquals(0, wasm.brOnCastGenericNullAnyArray(strObj));

  assertEquals(1, wasm.brOnCastGenericNullStruct(null));
  assertEquals(0, wasm.brOnCastGenericNullStruct(undefined));
  assertEquals(1, wasm.brOnCastGenericNullStruct(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericNullStruct(structSubObj));
  assertEquals(0, wasm.brOnCastGenericNullStruct(arrayObj));
  assertEquals(0, wasm.brOnCastGenericNullStruct(funcObj));
  assertEquals(0, wasm.brOnCastGenericNullStruct(1));
  assertEquals(0, wasm.brOnCastGenericNullStruct(jsObj));
  assertEquals(0, wasm.brOnCastGenericNullStruct(strObj));

  assertEquals(1, wasm.brOnCastGenericNullEq(null));
  assertEquals(0, wasm.brOnCastGenericNullEq(undefined));
  assertEquals(1, wasm.brOnCastGenericNullEq(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericNullEq(structSubObj));
  assertEquals(1, wasm.brOnCastGenericNullEq(arrayObj));
  assertEquals(0, wasm.brOnCastGenericNullEq(funcObj));
  assertEquals(1, wasm.brOnCastGenericNullEq(1));
  assertEquals(0, wasm.brOnCastGenericNullEq(jsObj));
  assertEquals(0, wasm.brOnCastGenericNullEq(strObj));

  assertEquals(1, wasm.brOnCastGenericNullString(null));
  assertEquals(0, wasm.brOnCastGenericNullString(undefined));
  assertEquals(0, wasm.brOnCastGenericNullString(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericNullString(structSubObj));
  assertEquals(0, wasm.brOnCastGenericNullString(arrayObj));
  assertEquals(0, wasm.brOnCastGenericNullString(funcObj));
  assertEquals(0, wasm.brOnCastGenericNullString(1));
  assertEquals(0, wasm.brOnCastGenericNullString(jsObj));
  assertEquals(1, wasm.brOnCastGenericNullString(strObj));

  assertEquals(1, wasm.brOnCastGenericNullAny(null));
  assertEquals(1, wasm.brOnCastGenericNullAny(undefined));
  assertEquals(1, wasm.brOnCastGenericNullAny(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericNullAny(structSubObj));
  assertEquals(1, wasm.brOnCastGenericNullAny(arrayObj));
  assertEquals(1, wasm.brOnCastGenericNullAny(funcObj));
  assertEquals(1, wasm.brOnCastGenericNullAny(1));
  assertEquals(1, wasm.brOnCastGenericNullAny(jsObj));
  assertEquals(1, wasm.brOnCastGenericNullAny(strObj));

  assertEquals(1, wasm.brOnCastGenericNullNone(null));
  assertEquals(0, wasm.brOnCastGenericNullNone(undefined));
  assertEquals(0, wasm.brOnCastGenericNullNone(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericNullNone(structSubObj));
  assertEquals(0, wasm.brOnCastGenericNullNone(arrayObj));
  assertEquals(0, wasm.brOnCastGenericNullNone(funcObj));
  assertEquals(0, wasm.brOnCastGenericNullNone(1));
  assertEquals(0, wasm.brOnCastGenericNullNone(jsObj));
  assertEquals(0, wasm.brOnCastGenericNullNone(strObj));

  // br_on_cast_fail
  assertEquals(1, wasm.brOnCastGenericFailStructSuper(null));
  assertEquals(1, wasm.brOnCastGenericFailStructSuper(undefined));
  assertEquals(0, wasm.brOnCastGenericFailStructSuper(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericFailStructSuper(structSubObj));
  assertEquals(1, wasm.brOnCastGenericFailStructSuper(arrayObj));
  assertEquals(1, wasm.brOnCastGenericFailStructSuper(funcObj));
  assertEquals(1, wasm.brOnCastGenericFailStructSuper(1));
  assertEquals(1, wasm.brOnCastGenericFailStructSuper(jsObj));
  assertEquals(1, wasm.brOnCastGenericFailStructSuper(strObj));

  assertEquals(1, wasm.brOnCastGenericFailStructSub(null));
  assertEquals(1, wasm.brOnCastGenericFailStructSub(undefined));
  assertEquals(1, wasm.brOnCastGenericFailStructSub(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericFailStructSub(structSubObj));
  assertEquals(1, wasm.brOnCastGenericFailStructSub(arrayObj));
  assertEquals(1, wasm.brOnCastGenericFailStructSub(funcObj));
  assertEquals(1, wasm.brOnCastGenericFailStructSub(1));
  assertEquals(1, wasm.brOnCastGenericFailStructSub(jsObj));
  assertEquals(1, wasm.brOnCastGenericFailStructSub(strObj));

  assertEquals(1, wasm.brOnCastGenericFailArray(null));
  assertEquals(1, wasm.brOnCastGenericFailArray(undefined));
  assertEquals(1, wasm.brOnCastGenericFailArray(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericFailArray(structSubObj));
  assertEquals(0, wasm.brOnCastGenericFailArray(arrayObj));
  assertEquals(1, wasm.brOnCastGenericFailArray(funcObj));
  assertEquals(1, wasm.brOnCastGenericFailArray(1));
  assertEquals(1, wasm.brOnCastGenericFailArray(jsObj));
  assertEquals(1, wasm.brOnCastGenericFailArray(strObj));

  assertEquals(1, wasm.brOnCastGenericFailI31(null));
  assertEquals(1, wasm.brOnCastGenericFailI31(undefined));
  assertEquals(1, wasm.brOnCastGenericFailI31(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericFailI31(structSubObj));
  assertEquals(1, wasm.brOnCastGenericFailI31(arrayObj));
  assertEquals(1, wasm.brOnCastGenericFailI31(funcObj));
  assertEquals(0, wasm.brOnCastGenericFailI31(1));
  assertEquals(1, wasm.brOnCastGenericFailI31(jsObj));
  assertEquals(1, wasm.brOnCastGenericFailI31(strObj));

  assertEquals(1, wasm.brOnCastGenericFailAnyArray(null));
  assertEquals(1, wasm.brOnCastGenericFailAnyArray(undefined));
  assertEquals(1, wasm.brOnCastGenericFailAnyArray(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericFailAnyArray(structSubObj));
  assertEquals(0, wasm.brOnCastGenericFailAnyArray(arrayObj));
  assertEquals(1, wasm.brOnCastGenericFailAnyArray(funcObj));
  assertEquals(1, wasm.brOnCastGenericFailAnyArray(1));
  assertEquals(1, wasm.brOnCastGenericFailAnyArray(jsObj));
  assertEquals(1, wasm.brOnCastGenericFailAnyArray(strObj));

  assertEquals(1, wasm.brOnCastGenericFailStruct(null));
  assertEquals(1, wasm.brOnCastGenericFailStruct(undefined));
  assertEquals(0, wasm.brOnCastGenericFailStruct(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericFailStruct(structSubObj));
  assertEquals(1, wasm.brOnCastGenericFailStruct(arrayObj));
  assertEquals(1, wasm.brOnCastGenericFailStruct(funcObj));
  assertEquals(1, wasm.brOnCastGenericFailStruct(1));
  assertEquals(1, wasm.brOnCastGenericFailStruct(jsObj));
  assertEquals(1, wasm.brOnCastGenericFailStruct(strObj));

  assertEquals(1, wasm.brOnCastGenericFailEq(null));
  assertEquals(1, wasm.brOnCastGenericFailEq(undefined));
  assertEquals(0, wasm.brOnCastGenericFailEq(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericFailEq(structSubObj));
  assertEquals(0, wasm.brOnCastGenericFailEq(arrayObj));
  assertEquals(1, wasm.brOnCastGenericFailEq(funcObj));
  assertEquals(0, wasm.brOnCastGenericFailEq(1));
  assertEquals(1, wasm.brOnCastGenericFailEq(jsObj));
  assertEquals(1, wasm.brOnCastGenericFailEq(strObj));

  assertEquals(1, wasm.brOnCastGenericFailString(null));
  assertEquals(1, wasm.brOnCastGenericFailString(undefined));
  assertEquals(1, wasm.brOnCastGenericFailString(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericFailString(structSubObj));
  assertEquals(1, wasm.brOnCastGenericFailString(arrayObj));
  assertEquals(1, wasm.brOnCastGenericFailString(funcObj));
  assertEquals(1, wasm.brOnCastGenericFailString(1));
  assertEquals(1, wasm.brOnCastGenericFailString(jsObj));
  assertEquals(0, wasm.brOnCastGenericFailString(strObj));

  assertEquals(1, wasm.brOnCastGenericFailAny(null));
  assertEquals(0, wasm.brOnCastGenericFailAny(undefined));
  assertEquals(0, wasm.brOnCastGenericFailAny(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericFailAny(structSubObj));
  assertEquals(0, wasm.brOnCastGenericFailAny(arrayObj));
  assertEquals(0, wasm.brOnCastGenericFailAny(funcObj));
  assertEquals(0, wasm.brOnCastGenericFailAny(1));
  assertEquals(0, wasm.brOnCastGenericFailAny(jsObj));
  assertEquals(0, wasm.brOnCastGenericFailAny(strObj));

  assertEquals(1, wasm.brOnCastGenericFailNone(null));
  assertEquals(1, wasm.brOnCastGenericFailNone(undefined));
  assertEquals(1, wasm.brOnCastGenericFailNone(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericFailNone(structSubObj));
  assertEquals(1, wasm.brOnCastGenericFailNone(arrayObj));
  assertEquals(1, wasm.brOnCastGenericFailNone(funcObj));
  assertEquals(1, wasm.brOnCastGenericFailNone(1));
  assertEquals(1, wasm.brOnCastGenericFailNone(jsObj));
  assertEquals(1, wasm.brOnCastGenericFailNone(strObj));

  // br_on_cast_fail null
  assertEquals(0, wasm.brOnCastGenericFailNullStructSuper(null));
  assertEquals(1, wasm.brOnCastGenericFailNullStructSuper(undefined));
  assertEquals(0, wasm.brOnCastGenericFailNullStructSuper(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericFailNullStructSuper(structSubObj));
  assertEquals(1, wasm.brOnCastGenericFailNullStructSuper(arrayObj));
  assertEquals(1, wasm.brOnCastGenericFailNullStructSuper(funcObj));
  assertEquals(1, wasm.brOnCastGenericFailNullStructSuper(1));
  assertEquals(1, wasm.brOnCastGenericFailNullStructSuper(jsObj));
  assertEquals(1, wasm.brOnCastGenericFailNullStructSuper(strObj));

  assertEquals(0, wasm.brOnCastGenericFailNullStructSub(null));
  assertEquals(1, wasm.brOnCastGenericFailNullStructSub(undefined));
  assertEquals(1, wasm.brOnCastGenericFailNullStructSub(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericFailNullStructSub(structSubObj));
  assertEquals(1, wasm.brOnCastGenericFailNullStructSub(arrayObj));
  assertEquals(1, wasm.brOnCastGenericFailNullStructSub(funcObj));
  assertEquals(1, wasm.brOnCastGenericFailNullStructSub(1));
  assertEquals(1, wasm.brOnCastGenericFailNullStructSub(jsObj));
  assertEquals(1, wasm.brOnCastGenericFailNullStructSub(strObj));

  assertEquals(0, wasm.brOnCastGenericFailNullArray(null));
  assertEquals(1, wasm.brOnCastGenericFailNullArray(undefined));
  assertEquals(1, wasm.brOnCastGenericFailNullArray(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericFailNullArray(structSubObj));
  assertEquals(0, wasm.brOnCastGenericFailNullArray(arrayObj));
  assertEquals(1, wasm.brOnCastGenericFailNullArray(funcObj));
  assertEquals(1, wasm.brOnCastGenericFailNullArray(1));
  assertEquals(1, wasm.brOnCastGenericFailNullArray(jsObj));
  assertEquals(1, wasm.brOnCastGenericFailNullArray(strObj));

  assertEquals(0, wasm.brOnCastGenericFailNullI31(null));
  assertEquals(1, wasm.brOnCastGenericFailNullI31(undefined));
  assertEquals(1, wasm.brOnCastGenericFailNullI31(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericFailNullI31(structSubObj));
  assertEquals(1, wasm.brOnCastGenericFailNullI31(arrayObj));
  assertEquals(1, wasm.brOnCastGenericFailNullI31(funcObj));
  assertEquals(0, wasm.brOnCastGenericFailNullI31(1));
  assertEquals(1, wasm.brOnCastGenericFailNullI31(jsObj));
  assertEquals(1, wasm.brOnCastGenericFailNullI31(strObj));

  assertEquals(0, wasm.brOnCastGenericFailNullAnyArray(null));
  assertEquals(1, wasm.brOnCastGenericFailNullAnyArray(undefined));
  assertEquals(1, wasm.brOnCastGenericFailNullAnyArray(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericFailNullAnyArray(structSubObj));
  assertEquals(0, wasm.brOnCastGenericFailNullAnyArray(arrayObj));
  assertEquals(1, wasm.brOnCastGenericFailNullAnyArray(funcObj));
  assertEquals(1, wasm.brOnCastGenericFailNullAnyArray(1));
  assertEquals(1, wasm.brOnCastGenericFailNullAnyArray(jsObj));
  assertEquals(1, wasm.brOnCastGenericFailNullAnyArray(strObj));

  assertEquals(0, wasm.brOnCastGenericFailNullStruct(null));
  assertEquals(1, wasm.brOnCastGenericFailNullStruct(undefined));
  assertEquals(0, wasm.brOnCastGenericFailNullStruct(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericFailNullStruct(structSubObj));
  assertEquals(1, wasm.brOnCastGenericFailNullStruct(arrayObj));
  assertEquals(1, wasm.brOnCastGenericFailNullStruct(funcObj));
  assertEquals(1, wasm.brOnCastGenericFailNullStruct(1));
  assertEquals(1, wasm.brOnCastGenericFailNullStruct(jsObj));
  assertEquals(1, wasm.brOnCastGenericFailNullStruct(strObj));

  assertEquals(0, wasm.brOnCastGenericFailNullEq(null));
  assertEquals(1, wasm.brOnCastGenericFailNullEq(undefined));
  assertEquals(0, wasm.brOnCastGenericFailNullEq(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericFailNullEq(structSubObj));
  assertEquals(0, wasm.brOnCastGenericFailNullEq(arrayObj));
  assertEquals(1, wasm.brOnCastGenericFailNullEq(funcObj));
  assertEquals(0, wasm.brOnCastGenericFailNullEq(1));
  assertEquals(1, wasm.brOnCastGenericFailNullEq(jsObj));
  assertEquals(1, wasm.brOnCastGenericFailNullEq(strObj));

  assertEquals(0, wasm.brOnCastGenericFailNullString(null));
  assertEquals(1, wasm.brOnCastGenericFailNullString(undefined));
  assertEquals(1, wasm.brOnCastGenericFailNullString(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericFailNullString(structSubObj));
  assertEquals(1, wasm.brOnCastGenericFailNullString(arrayObj));
  assertEquals(1, wasm.brOnCastGenericFailNullString(funcObj));
  assertEquals(1, wasm.brOnCastGenericFailNullString(1));
  assertEquals(1, wasm.brOnCastGenericFailNullString(jsObj));
  assertEquals(0, wasm.brOnCastGenericFailNullString(strObj));

  assertEquals(0, wasm.brOnCastGenericFailNullAny(null));
  assertEquals(0, wasm.brOnCastGenericFailNullAny(undefined));
  assertEquals(0, wasm.brOnCastGenericFailNullAny(structSuperObj));
  assertEquals(0, wasm.brOnCastGenericFailNullAny(structSubObj));
  assertEquals(0, wasm.brOnCastGenericFailNullAny(arrayObj));
  assertEquals(0, wasm.brOnCastGenericFailNullAny(funcObj));
  assertEquals(0, wasm.brOnCastGenericFailNullAny(1));
  assertEquals(0, wasm.brOnCastGenericFailNullAny(jsObj));
  assertEquals(0, wasm.brOnCastGenericFailNullAny(strObj));

  assertEquals(0, wasm.brOnCastGenericFailNullNone(null));
  assertEquals(1, wasm.brOnCastGenericFailNullNone(undefined));
  assertEquals(1, wasm.brOnCastGenericFailNullNone(structSuperObj));
  assertEquals(1, wasm.brOnCastGenericFailNullNone(structSubObj));
  assertEquals(1, wasm.brOnCastGenericFailNullNone(arrayObj));
  assertEquals(1, wasm.brOnCastGenericFailNullNone(funcObj));
  assertEquals(1, wasm.brOnCastGenericFailNullNone(1));
  assertEquals(1, wasm.brOnCastGenericFailNullNone(jsObj));
  assertEquals(1, wasm.brOnCastGenericFailNullNone(strObj));
})();
