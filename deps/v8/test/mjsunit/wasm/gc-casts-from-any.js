// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestRefTest() {
  var builder = new WasmModuleBuilder();
  let structSuper = builder.addStruct([makeField(kWasmI32, true)]);
  let structSub = builder.addStruct([makeField(kWasmI32, true)], structSuper);
  let array = builder.addArray(kWasmI32);

  let fct =
  builder.addFunction('createStructSuper',
                      makeSig([kWasmI32], [kWasmExternRef]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStructNew, structSuper,
      kGCPrefix, kExprExternExternalize,
    ]).exportFunc();
  builder.addFunction('createStructSub', makeSig([kWasmI32], [kWasmExternRef]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStructNew, structSub,
      kGCPrefix, kExprExternExternalize,
    ]).exportFunc();
  builder.addFunction('createArray', makeSig([kWasmI32], [kWasmExternRef]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprArrayNewFixed, array, 1,
      kGCPrefix, kExprExternExternalize,
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
    ["Data", kDataRefCode],
    ["Eq", kEqRefCode],
    // 'ref.test any' is semantically the same as '!ref.is_null' here.
    ["Any", kAnyRefCode],
  ].forEach(([typeName, typeCode]) => {
    builder.addFunction(`refTest${typeName}`,
                        makeSig([kWasmExternRef], [kWasmI32, kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprExternInternalize,
      kGCPrefix, kExprRefTest, typeCode,
      kExprLocalGet, 0,
      kGCPrefix, kExprExternInternalize,
      kGCPrefix, kExprRefTestNull, typeCode,
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

  assertEquals([0, 1], wasm.refTestStructSub(null));
  assertEquals([0, 0], wasm.refTestStructSub(undefined));
  assertEquals([0, 0], wasm.refTestStructSub(wasm.createStructSuper()));
  assertEquals([1, 1], wasm.refTestStructSub(wasm.createStructSub()));
  assertEquals([0, 0], wasm.refTestStructSub(wasm.createArray()));
  assertEquals([0, 0], wasm.refTestStructSub(wasm.createFuncRef()));
  assertEquals([0, 0], wasm.refTestStructSub(1));
  assertEquals([0, 0], wasm.refTestStructSub({'JavaScript': 'Object'}));

  assertEquals([0, 1], wasm.refTestArray(null));
  assertEquals([0, 0], wasm.refTestArray(undefined));
  assertEquals([0, 0], wasm.refTestArray(wasm.createStructSuper()));
  assertEquals([0, 0], wasm.refTestArray(wasm.createStructSub()));
  assertEquals([1, 1], wasm.refTestArray(wasm.createArray()));
  assertEquals([0, 0], wasm.refTestArray(wasm.createFuncRef()));
  assertEquals([0, 0], wasm.refTestArray(1));
  assertEquals([0, 0], wasm.refTestArray({'JavaScript': 'Object'}));

  assertEquals([0, 1], wasm.refTestI31(null));
  assertEquals([0, 0], wasm.refTestI31(undefined));
  assertEquals([0, 0], wasm.refTestI31(wasm.createStructSuper()));
  assertEquals([0, 0], wasm.refTestI31(wasm.createStructSub()));
  assertEquals([0, 0], wasm.refTestI31(wasm.createArray()));
  assertEquals([0, 0], wasm.refTestI31(wasm.createFuncRef()));
  assertEquals([1, 1], wasm.refTestI31(1));
  assertEquals([0, 0], wasm.refTestI31({'JavaScript': 'Object'}));

  assertEquals([0, 1], wasm.refTestAnyArray(null));
  assertEquals([0, 0], wasm.refTestAnyArray(undefined));
  assertEquals([0, 0], wasm.refTestAnyArray(wasm.createStructSuper()));
  assertEquals([0, 0], wasm.refTestAnyArray(wasm.createStructSub()));
  assertEquals([1, 1], wasm.refTestAnyArray(wasm.createArray()));
  assertEquals([0, 0], wasm.refTestAnyArray(wasm.createFuncRef()));
  assertEquals([0, 0], wasm.refTestAnyArray(1));
  assertEquals([0, 0], wasm.refTestAnyArray({'JavaScript': 'Object'}));

  assertEquals([0, 1], wasm.refTestData(null));
  assertEquals([0, 0], wasm.refTestData(undefined));
  assertEquals([1, 1], wasm.refTestData(wasm.createStructSuper()));
  assertEquals([1, 1], wasm.refTestData(wasm.createStructSub()));
  assertEquals([1, 1], wasm.refTestData(wasm.createArray()));
  assertEquals([0, 0], wasm.refTestData(wasm.createFuncRef()));
  assertEquals([0, 0], wasm.refTestData(1));
  assertEquals([0, 0], wasm.refTestData({'JavaScript': 'Object'}));

  assertEquals([0, 1], wasm.refTestEq(null));
  assertEquals([0, 0], wasm.refTestEq(undefined));
  assertEquals([1, 1], wasm.refTestEq(wasm.createStructSuper()));
  assertEquals([1, 1], wasm.refTestEq(wasm.createStructSub()));
  assertEquals([1, 1], wasm.refTestEq(wasm.createArray()));
  assertEquals([0, 0], wasm.refTestEq(wasm.createFuncRef()));
  assertEquals([1, 1], wasm.refTestEq(1)); // ref.i31
  assertEquals([0, 0], wasm.refTestEq({'JavaScript': 'Object'}));

  assertEquals([0, 1], wasm.refTestAny(null));
  assertEquals([1, 1], wasm.refTestAny(undefined));
  assertEquals([1, 1], wasm.refTestAny(wasm.createStructSuper()));
  assertEquals([1, 1], wasm.refTestAny(wasm.createStructSub()));
  assertEquals([1, 1], wasm.refTestAny(wasm.createArray()));
  assertEquals([1, 1], wasm.refTestAny(wasm.createFuncRef()));
  assertEquals([1, 1], wasm.refTestAny(1)); // ref.i31
  assertEquals([1, 1], wasm.refTestAny({'JavaScript': 'Object'}));
})();
