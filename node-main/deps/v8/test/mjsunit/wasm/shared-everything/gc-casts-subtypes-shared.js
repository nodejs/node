// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-type-reflection --experimental-wasm-shared
// Flags: --harmony-struct

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Test casting null from one type to another using ref.test & ref.cast.
(function RefCastFromNull() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.startRecGroup();
  let structSuper = builder.addStruct([makeField(kWasmI32, true)], kNoSuperType, false, true);
  builder.endRecGroup();
  builder.startRecGroup();
  let structSub = builder.addStruct([makeField(kWasmI32, true)], structSuper, false, true);
  builder.endRecGroup();
  builder.startRecGroup();
  let array = builder.addArray(kWasmI32, true, kNoSuperType, false, true);
  builder.endRecGroup();

  // Note: Casting between unrelated types is allowed as long as the types
  // belong to the same type hierarchy (func / any / extern). In these cases the
  // check will always fail.
  // For br_on_cast, only downcasts are allowed!
  let tests = [
    [wasmRefNullType(kWasmAnyRef).shared(),
     wasmRefType(kWasmAnyRef).shared(), 'AnyToAny', true],
    // TODO(42204563): Enable when shared functions are supported.
    // [wasmRefNullType(kWasmFuncRef).shared(),
    //  wasmRefType(kWasmFuncRef).shared(), 'FuncToFunc', true],
    // [wasmRefNullType(kWasmNullFuncRef).shared(),
    //  wasmRefType(kWasmNullFuncRef).shared(), 'NullFuncToNullFunc', true],
    // [wasmRefNullType(kWasmFuncRef).shared(),
    //  wasmRefType(kWasmNullFuncRef).shared(), 'FuncToNullFunc', true],
    // [wasmRefNullType(kWasmNullFuncRef).shared(),
    //  wasmRefType(kWasmFuncRef).shared(), 'NullFuncToFunc', false],

    [wasmRefNullType(kWasmExternRef).shared(),
     wasmRefType(kWasmExternRef).shared(), 'ExternToExtern', true],
    [wasmRefNullType(kWasmNullExternRef).shared(),
     wasmRefType(kWasmNullExternRef).shared(), 'NullExternToNullExtern', true],
    [wasmRefNullType(structSub), wasmRefType(array), 'StructToArray', false],
    [wasmRefNullType(kWasmExternRef).shared(),
     wasmRefType(kWasmNullExternRef).shared(), 'ExternToNullExtern', true],
    [wasmRefNullType(kWasmNullExternRef).shared(),
     wasmRefType(kWasmExternRef).shared(), 'NullExternToExtern', false],
    [wasmRefNullType(kWasmNullRef).shared(),
     wasmRefType(kWasmAnyRef).shared(), 'NullToAny', false],
    [wasmRefNullType(kWasmI31Ref).shared(),
     wasmRefType(structSub), 'I31ToStruct', false],
    [wasmRefNullType(kWasmEqRef).shared(),
     wasmRefType(kWasmI31Ref).shared(), 'EqToI31', true],
    [wasmRefNullType(structSuper),
     wasmRefType(structSub), 'StructSuperToStructSub', true],
    [wasmRefNullType(structSub),
     wasmRefType(structSuper), 'StructSubToStructSuper', false],
  ];

  for (let [sourceType, targetType, testName, isDowncast] of tests) {
    builder.addFunction('testNull' + testName, makeSig([], [kWasmI32]))
    .addLocals(sourceType, 1)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefTest, ...wasmEncodeHeapType(targetType),
    ]).exportFunc();

    builder.addFunction('castNull' + testName, makeSig([], []))
    .addLocals(sourceType, 1)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefCast, ...wasmEncodeHeapType(targetType),
      kExprDrop,
    ]).exportFunc();

    if (isDowncast) {
      builder.addFunction('branchNull' + testName, makeSig([], [kWasmI32]))
      .addLocals(sourceType, 1)
      .addBody([
        kExprBlock, kWasmRef, ...wasmEncodeHeapType(targetType),
          kExprLocalGet, 0,
          ...wasmBrOnCast(0, sourceType, targetType),
          kExprI32Const, 0,
          kExprReturn,
        kExprEnd,
        kExprDrop,
        kExprI32Const, 1,
        kExprReturn,
      ]).exportFunc();

      builder.addFunction('branchFailNull' + testName, makeSig([], [kWasmI32]))
      .addLocals(sourceType, 1)
      .addBody([
        kExprBlock, kWasmRef, ...wasmEncodeHeapType(sourceType),
          kExprLocalGet, 0,
          ...wasmBrOnCastFail(0, sourceType, targetType.nullable()),
          kExprI32Const, 0,
          kExprReturn,
        kExprEnd,
        kExprDrop,
        kExprI32Const, 1,
        kExprReturn,
      ]).exportFunc();
    }
  }

  let instance = builder.instantiate();
  let wasm = instance.exports;

  for (let [sourceType, targetType, testName, isDowncast] of tests) {
    print(testName);
    assertEquals(0, wasm['testNull' + testName]());
    assertTraps(kTrapIllegalCast, wasm['castNull' + testName]);
    if (isDowncast) {
      assertEquals(0, wasm['branchNull' + testName]());
      assertEquals(0, wasm['branchFailNull' + testName]());
    }
  }
})();

// TODO(42204563): Adapt and enable when shared functions are supported.
// (function RefTestFuncRef() {
//   print(arguments.callee.name);
//   let builder = new WasmModuleBuilder();
//   let sigSuper = builder.addType(makeSig([kWasmI32], []), kNoSuperType, false);
//   let sigSub = builder.addType(makeSig([kWasmI32], []), sigSuper);

//   builder.addFunction('fctSuper', sigSuper).addBody([]).exportFunc();
//   builder.addFunction('fctSub', sigSub).addBody([]).exportFunc();
//   builder.addFunction('testFromFuncRef',
//       makeSig([kWasmFuncRef], [kWasmI32, kWasmI32, kWasmI32, kWasmI32]))
//     .addBody([
//       kExprLocalGet, 0, kGCPrefix, kExprRefTest, kFuncRefCode,
//       kExprLocalGet, 0, kGCPrefix, kExprRefTest, kNullFuncRefCode,
//       kExprLocalGet, 0, kGCPrefix, kExprRefTest, sigSuper,
//       kExprLocalGet, 0, kGCPrefix, kExprRefTest, sigSub,
//     ]).exportFunc();
//   builder.addFunction('testNullFromFuncRef',
//       makeSig([kWasmFuncRef], [kWasmI32, kWasmI32, kWasmI32, kWasmI32]))
//     .addBody([
//       kExprLocalGet, 0, kGCPrefix, kExprRefTestNull, kFuncRefCode,
//       kExprLocalGet, 0, kGCPrefix, kExprRefTestNull, kNullFuncRefCode,
//       kExprLocalGet, 0, kGCPrefix, kExprRefTestNull, sigSuper,
//       kExprLocalGet, 0, kGCPrefix, kExprRefTestNull, sigSub,
//     ]).exportFunc();

//   let instance = builder.instantiate();
//   let wasm = instance.exports;
//   let jsFct = new WebAssembly.Function(
//       {parameters:['i32', 'i32'], results: ['i32']},
//       function mul(a, b) { return a * b; });
//   assertEquals([0, 0, 0, 0], wasm.testFromFuncRef(null));
//   assertEquals([1, 0, 0, 0], wasm.testFromFuncRef(jsFct));
//   assertEquals([1, 0, 1, 0], wasm.testFromFuncRef(wasm.fctSuper));
//   assertEquals([1, 0, 1, 1], wasm.testFromFuncRef(wasm.fctSub));

//   assertEquals([1, 1, 1, 1], wasm.testNullFromFuncRef(null));
//   assertEquals([1, 0, 0, 0], wasm.testNullFromFuncRef(jsFct));
//   assertEquals([1, 0, 1, 0], wasm.testNullFromFuncRef(wasm.fctSuper));
//   assertEquals([1, 0, 1, 1], wasm.testNullFromFuncRef(wasm.fctSub));
// })();

// (function RefCastFuncRef() {
//   print(arguments.callee.name);
//   let builder = new WasmModuleBuilder();
//   let sigSuper = builder.addType(makeSig([kWasmI32], []), kNoSuperType, false);
//   let sigSub = builder.addType(makeSig([kWasmI32], []), sigSuper);

//   builder.addFunction('fctSuper', sigSuper).addBody([]).exportFunc();
//   builder.addFunction('fctSub', sigSub).addBody([]).exportFunc();
//   builder.addFunction('castToFuncRef', makeSig([kWasmFuncRef], [kWasmFuncRef]))
//     .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCast, kFuncRefCode])
//     .exportFunc();
//   builder.addFunction('castToNullFuncRef',
//                       makeSig([kWasmFuncRef], [kWasmFuncRef]))
//     .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCast, kNullFuncRefCode])
//     .exportFunc();
//   builder.addFunction('castToSuper', makeSig([kWasmFuncRef], [kWasmFuncRef]))
//     .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCast, sigSuper])
//     .exportFunc();
//   builder.addFunction('castToSub', makeSig([kWasmFuncRef], [kWasmFuncRef]))
//     .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCast, sigSub])
//     .exportFunc();

//   builder.addFunction('castNullToFuncRef',
//                       makeSig([kWasmFuncRef], [kWasmFuncRef]))
//     .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCastNull, kFuncRefCode])
//     .exportFunc();
//   builder.addFunction('castNullToNullFuncRef',
//                       makeSig([kWasmFuncRef], [kWasmFuncRef]))
//     .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCastNull, kNullFuncRefCode])
//     .exportFunc();
//   builder.addFunction('castNullToSuper',
//                       makeSig([kWasmFuncRef], [kWasmFuncRef]))
//     .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCastNull, sigSuper])
//     .exportFunc();
//   builder.addFunction('castNullToSub', makeSig([kWasmFuncRef], [kWasmFuncRef]))
//     .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCastNull, sigSub])
//     .exportFunc();

//   let instance = builder.instantiate();
//   let wasm = instance.exports;
//   let jsFct = new WebAssembly.Function(
//       {parameters:['i32', 'i32'], results: ['i32']},
//       function mul(a, b) { return a * b; });

//   assertTraps(kTrapIllegalCast, () => wasm.castToFuncRef(null));
//   assertSame(jsFct, wasm.castToFuncRef(jsFct));
//   assertSame(wasm.fctSuper, wasm.castToFuncRef(wasm.fctSuper));
//   assertSame(wasm.fctSub, wasm.castToFuncRef(wasm.fctSub));

//   assertSame(null, wasm.castNullToFuncRef(null));
//   assertSame(jsFct, wasm.castNullToFuncRef(jsFct));
//   assertSame(wasm.fctSuper, wasm.castNullToFuncRef(wasm.fctSuper));
//   assertSame(wasm.fctSub, wasm.castNullToFuncRef(wasm.fctSub));

//   assertSame(null, wasm.castNullToNullFuncRef(null));
//   assertTraps(kTrapIllegalCast, () => wasm.castNullToNullFuncRef(jsFct));
//   assertTraps(kTrapIllegalCast,
//               () => wasm.castNullToNullFuncRef(wasm.fctSuper));
//   assertTraps(kTrapIllegalCast, () => wasm.castNullToNullFuncRef(wasm.fctSub));

//   assertTraps(kTrapIllegalCast, () => wasm.castToSuper(null));
//   assertTraps(kTrapIllegalCast, () => wasm.castToSuper(jsFct));
//   assertSame(wasm.fctSuper, wasm.castToSuper(wasm.fctSuper));
//   assertSame(wasm.fctSub, wasm.castToSuper(wasm.fctSub));

//   assertSame(null, wasm.castNullToSuper(null));
//   assertTraps(kTrapIllegalCast, () => wasm.castNullToSuper(jsFct));
//   assertSame(wasm.fctSuper, wasm.castNullToSuper(wasm.fctSuper));
//   assertSame(wasm.fctSub, wasm.castNullToSuper(wasm.fctSub));

//   assertTraps(kTrapIllegalCast, () => wasm.castToSub(null));
//   assertTraps(kTrapIllegalCast, () => wasm.castToSub(jsFct));
//   assertTraps(kTrapIllegalCast, () => wasm.castToSub(wasm.fctSuper));
//   assertSame(wasm.fctSub, wasm.castToSub(wasm.fctSub));

//   assertSame(null, wasm.castNullToSub(null));
//   assertTraps(kTrapIllegalCast, () => wasm.castNullToSub(jsFct));
//   assertTraps(kTrapIllegalCast, () => wasm.castNullToSub(wasm.fctSuper));
//   assertSame(wasm.fctSub, wasm.castNullToSub(wasm.fctSub));
// })();

// (function BrOnCastFuncRef() {
//   print(arguments.callee.name);
//   let builder = new WasmModuleBuilder();
//   let sigSuper = builder.addType(makeSig([kWasmI32], []), kNoSuperType, false);
//   let sigSub = builder.addType(makeSig([kWasmI32], []), sigSuper);

//   builder.addFunction('fctSuper', sigSuper).addBody([]).exportFunc();
//   builder.addFunction('fctSub', sigSub).addBody([]).exportFunc();
//   let targets = {
//     "funcref": kWasmFuncRef,
//     "nullfuncref": kWasmNullFuncRef,
//     "super": sigSuper,
//     "sub": sigSub
//   };
//   for (const [name, target_type] of Object.entries(targets)) {
//     builder.addFunction(`brOnCast_${name}`,
//       makeSig([kWasmFuncRef], [kWasmI32]))
//     .addBody([
//       kExprBlock, kWasmRef, target_type & kLeb128Mask,
//         kExprLocalGet, 0,
//         ...wasmBrOnCast(
//             0, wasmRefNullType(kWasmFuncRef), wasmRefType(target_type)),
//         kExprI32Const, 0,
//         kExprReturn,
//       kExprEnd,
//       kExprDrop,
//       kExprI32Const, 1,
//       kExprReturn,
//     ]).exportFunc();

//     builder.addFunction(`brOnCastNull_${name}`,
//       makeSig([kWasmFuncRef], [kWasmI32]))
//     .addBody([
//       kExprBlock, kWasmRefNull, target_type & kLeb128Mask,
//         kExprLocalGet, 0,
//         ...wasmBrOnCast(
//             0, wasmRefNullType(kWasmFuncRef), wasmRefNullType(target_type)),
//         kExprI32Const, 0,
//         kExprReturn,
//       kExprEnd,
//       kExprDrop,
//       kExprI32Const, 1,
//       kExprReturn,
//     ]).exportFunc();

//     builder.addFunction(`brOnCastFail_${name}`,
//       makeSig([kWasmFuncRef], [kWasmI32]))
//     .addBody([
//       kExprBlock, kFuncRefCode,
//         kExprLocalGet, 0,
//         ...wasmBrOnCastFail(
//             0, wasmRefNullType(kWasmFuncRef), wasmRefType(target_type)),
//         kExprI32Const, 0,
//         kExprReturn,
//       kExprEnd,
//       kExprDrop,
//       kExprI32Const, 1,
//       kExprReturn,
//     ]).exportFunc();

//     builder.addFunction(`brOnCastFailNull_${name}`,
//       makeSig([kWasmFuncRef], [kWasmI32]))
//     .addBody([
//       kExprBlock, kWasmRef, kFuncRefCode,
//         kExprLocalGet, 0,
//         ...wasmBrOnCastFail(
//           0, wasmRefNullType(kWasmFuncRef), wasmRefNullType(target_type)),
//         kExprI32Const, 0,
//         kExprReturn,
//       kExprEnd,
//       kExprDrop,
//       kExprI32Const, 1,
//       kExprReturn,
//     ]).exportFunc();
//   }

//   let instance = builder.instantiate();
//   let wasm = instance.exports;
//   let jsFct = new WebAssembly.Function(
//       {parameters:['i32', 'i32'], results: ['i32']},
//       function mul(a, b) { return a * b; });
//   assertEquals(0, wasm.brOnCast_funcref(null));
//   assertEquals(0, wasm.brOnCast_nullfuncref(null));
//   assertEquals(0, wasm.brOnCast_super(null));
//   assertEquals(0, wasm.brOnCast_sub(null));

//   assertEquals(1, wasm.brOnCast_funcref(jsFct));
//   assertEquals(0, wasm.brOnCast_nullfuncref(jsFct));
//   assertEquals(0, wasm.brOnCast_super(jsFct));
//   assertEquals(0, wasm.brOnCast_sub(jsFct));

//   assertEquals(1, wasm.brOnCast_funcref(wasm.fctSuper));
//   assertEquals(0, wasm.brOnCast_nullfuncref(wasm.fctSuper));
//   assertEquals(1, wasm.brOnCast_super(wasm.fctSuper));
//   assertEquals(0, wasm.brOnCast_sub(wasm.fctSuper));

//   assertEquals(1, wasm.brOnCast_funcref(wasm.fctSub));
//   assertEquals(0, wasm.brOnCast_nullfuncref(wasm.fctSub));
//   assertEquals(1, wasm.brOnCast_super(wasm.fctSub));
//   assertEquals(1, wasm.brOnCast_sub(wasm.fctSub));

//   // br_on_cast null
//   assertEquals(1, wasm.brOnCastNull_funcref(null));
//   assertEquals(1, wasm.brOnCastNull_nullfuncref(null));
//   assertEquals(1, wasm.brOnCastNull_super(null));
//   assertEquals(1, wasm.brOnCastNull_sub(null));

//   assertEquals(1, wasm.brOnCastNull_funcref(jsFct));
//   assertEquals(0, wasm.brOnCastNull_nullfuncref(jsFct));
//   assertEquals(0, wasm.brOnCastNull_super(jsFct));
//   assertEquals(0, wasm.brOnCastNull_sub(jsFct));

//   assertEquals(1, wasm.brOnCastNull_funcref(wasm.fctSuper));
//   assertEquals(0, wasm.brOnCastNull_nullfuncref(wasm.fctSuper));
//   assertEquals(1, wasm.brOnCastNull_super(wasm.fctSuper));
//   assertEquals(0, wasm.brOnCastNull_sub(wasm.fctSuper));

//   assertEquals(1, wasm.brOnCastNull_funcref(wasm.fctSub));
//   assertEquals(0, wasm.brOnCastNull_nullfuncref(wasm.fctSub));
//   assertEquals(1, wasm.brOnCastNull_super(wasm.fctSub));
//   assertEquals(1, wasm.brOnCastNull_sub(wasm.fctSub));

//   // br_on_cast_fail
//   assertEquals(1, wasm.brOnCastFail_funcref(null));
//   assertEquals(1, wasm.brOnCastFail_nullfuncref(null));
//   assertEquals(1, wasm.brOnCastFail_super(null));
//   assertEquals(1, wasm.brOnCastFail_sub(null));

//   assertEquals(0, wasm.brOnCastFail_funcref(jsFct));
//   assertEquals(1, wasm.brOnCastFail_nullfuncref(jsFct));
//   assertEquals(1, wasm.brOnCastFail_super(jsFct));
//   assertEquals(1, wasm.brOnCastFail_sub(jsFct));

//   assertEquals(0, wasm.brOnCastFail_funcref(wasm.fctSuper));
//   assertEquals(1, wasm.brOnCastFail_nullfuncref(wasm.fctSuper));
//   assertEquals(0, wasm.brOnCastFail_super(wasm.fctSuper));
//   assertEquals(1, wasm.brOnCastFail_sub(wasm.fctSuper));

//   assertEquals(0, wasm.brOnCastFail_funcref(wasm.fctSub));
//   assertEquals(1, wasm.brOnCastFail_nullfuncref(wasm.fctSub));
//   assertEquals(0, wasm.brOnCastFail_super(wasm.fctSub));
//   assertEquals(0, wasm.brOnCastFail_sub(wasm.fctSub));

//   // br_on_cast_fail null
//   assertEquals(0, wasm.brOnCastFailNull_funcref(null));
//   assertEquals(0, wasm.brOnCastFailNull_nullfuncref(null));
//   assertEquals(0, wasm.brOnCastFailNull_super(null));
//   assertEquals(0, wasm.brOnCastFailNull_sub(null));

//   assertEquals(0, wasm.brOnCastFailNull_funcref(jsFct));
//   assertEquals(1, wasm.brOnCastFailNull_nullfuncref(jsFct));
//   assertEquals(1, wasm.brOnCastFailNull_super(jsFct));
//   assertEquals(1, wasm.brOnCastFailNull_sub(jsFct));

//   assertEquals(0, wasm.brOnCastFailNull_funcref(wasm.fctSuper));
//   assertEquals(1, wasm.brOnCastFailNull_nullfuncref(wasm.fctSuper));
//   assertEquals(0, wasm.brOnCastFailNull_super(wasm.fctSuper));
//   assertEquals(1, wasm.brOnCastFailNull_sub(wasm.fctSuper));

//   assertEquals(0, wasm.brOnCastFailNull_funcref(wasm.fctSub));
//   assertEquals(1, wasm.brOnCastFailNull_nullfuncref(wasm.fctSub));
//   assertEquals(0, wasm.brOnCastFailNull_super(wasm.fctSub));
//   assertEquals(0, wasm.brOnCastFailNull_sub(wasm.fctSub));
// })();

(function RefTestExternRef() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sharedExternRef = wasmRefNullType(kWasmExternRef).shared();
  let sharedNullExternRef = wasmRefNullType(kWasmNullExternRef).shared();
  let sharedObj = new (new SharedStructType(["prop"]));

  builder.addFunction('testExternRef',
      makeSig([sharedExternRef], [kWasmI32, kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefTest, ...wasmEncodeHeapType(sharedExternRef),
      kExprLocalGet, 0,
      kGCPrefix, kExprRefTest, ...wasmEncodeHeapType(sharedNullExternRef),
    ]).exportFunc();

  builder.addFunction('testNullExternRef',
      makeSig([sharedExternRef], [kWasmI32, kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefTestNull, ...wasmEncodeHeapType(sharedExternRef),
      kExprLocalGet, 0,
      kGCPrefix, kExprRefTestNull, ...wasmEncodeHeapType(sharedNullExternRef),
    ]).exportFunc();

  let instance = builder.instantiate();
  let wasm = instance.exports;
  assertEquals([0, 0], wasm.testExternRef(null));
  assertEquals([1, 0], wasm.testExternRef(undefined));
  assertEquals([1, 0], wasm.testExternRef(1));
  assertEquals([1, 0], wasm.testExternRef("test"));
  assertEquals([1, 0], wasm.testExternRef(sharedObj));

  assertEquals([1, 1], wasm.testNullExternRef(null));
  assertEquals([1, 0], wasm.testNullExternRef(undefined));
  assertEquals([1, 0], wasm.testNullExternRef(1));
  assertEquals([1, 0], wasm.testNullExternRef("test"));
  assertEquals([1, 0], wasm.testNullExternRef(sharedObj));
})();

(function RefCastExternRef() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sharedExternRef = wasmRefNullType(kWasmExternRef).shared();
  let sharedNullExternRef = wasmRefNullType(kWasmNullExternRef).shared();

  builder.addFunction('castToExternRef',
      makeSig([sharedExternRef], [sharedExternRef]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefCast, ...wasmEncodeHeapType(sharedExternRef)
    ])
    .exportFunc();
  builder.addFunction('castToNullExternRef',
    makeSig([sharedExternRef], [sharedExternRef]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprRefCast, ...wasmEncodeHeapType(sharedNullExternRef)
  ])
  .exportFunc();
  builder.addFunction('castNullToExternRef',
    makeSig([sharedExternRef], [sharedExternRef]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprRefCastNull, ...wasmEncodeHeapType(sharedExternRef)
  ])
  .exportFunc();
  builder.addFunction('castNullToNullExternRef',
    makeSig([sharedExternRef], [sharedExternRef]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprRefCastNull, ...wasmEncodeHeapType(sharedNullExternRef)
  ])
  .exportFunc();

  let instance = builder.instantiate();
  let wasm = instance.exports;

  assertTraps(kTrapIllegalCast, () => wasm.castToExternRef(null));
  assertEquals(undefined, wasm.castToExternRef(undefined));
  assertEquals(1, wasm.castToExternRef(1));
  let obj = new (new SharedStructType(["prop"]));
  assertSame(obj, wasm.castToExternRef(obj));

  assertTraps(kTrapIllegalCast, () => wasm.castToNullExternRef(null));
  assertTraps(kTrapIllegalCast, () => wasm.castToNullExternRef(undefined));
  assertTraps(kTrapIllegalCast, () => wasm.castToNullExternRef(1));
  assertTraps(kTrapIllegalCast, () => wasm.castToNullExternRef(obj));

  assertSame(null, wasm.castNullToExternRef(null));
  assertEquals(undefined, wasm.castNullToExternRef(undefined));
  assertEquals(1, wasm.castNullToExternRef(1));
  assertSame(obj, wasm.castNullToExternRef(obj));

  assertSame(null, wasm.castNullToNullExternRef(null));
  assertTraps(kTrapIllegalCast, () => wasm.castNullToNullExternRef(undefined));
  assertTraps(kTrapIllegalCast, () => wasm.castNullToNullExternRef(1));
  assertTraps(kTrapIllegalCast, () => wasm.castNullToNullExternRef(obj));
})();

(function BrOnCastExternRef() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sharedExternRef = wasmRefNullType(kWasmExternRef).shared();
  let sharedNullExternRef = wasmRefNullType(kWasmNullExternRef).shared();

  builder.addFunction('castToExternRef',
    makeSig([sharedExternRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRef, ...wasmEncodeHeapType(sharedExternRef),
      kExprLocalGet, 0,
      ...wasmBrOnCast(0,
        wasmRefNullType(kWasmExternRef).shared(),
        wasmRefType(kWasmExternRef).shared()),
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();
  builder.addFunction('castToNullExternRef',
    makeSig([sharedExternRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRef, ...wasmEncodeHeapType(sharedNullExternRef),
      kExprLocalGet, 0,
      ...wasmBrOnCast(0,
        wasmRefNullType(kWasmExternRef).shared(),
        wasmRefType(kWasmNullExternRef).shared()),
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();

  builder.addFunction('castNullToExternRef',
    makeSig([sharedExternRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRefNull, ...wasmEncodeHeapType(sharedExternRef),
      kExprLocalGet, 0,
      ...wasmBrOnCast(0,
          wasmRefNullType(kWasmExternRef).shared(),
          wasmRefNullType(kWasmExternRef).shared()),
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();
  builder.addFunction('castNullToNullExternRef',
    makeSig([sharedExternRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRefNull, ...wasmEncodeHeapType(sharedNullExternRef),
      kExprLocalGet, 0,
      ...wasmBrOnCast(0,
          wasmRefNullType(kWasmExternRef).shared(),
          wasmRefNullType(kWasmNullExternRef).shared()),
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();

  builder.addFunction('castFailToExternRef',
    makeSig([sharedExternRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRefNull, ...wasmEncodeHeapType(sharedExternRef),
      kExprLocalGet, 0,
      ...wasmBrOnCastFail(0,
          wasmRefNullType(kWasmExternRef).shared(),
          wasmRefType(kWasmExternRef).shared()),
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();
  builder.addFunction('castFailToNullExternRef',
    makeSig([sharedExternRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRefNull, ...wasmEncodeHeapType(sharedExternRef),
      kExprLocalGet, 0,
      ...wasmBrOnCastFail(0,
          wasmRefNullType(kWasmExternRef).shared(),
          wasmRefType(kWasmNullExternRef).shared()),
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();

  builder.addFunction('castFailNullToExternRef',
    makeSig([sharedExternRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRef, ...wasmEncodeHeapType(sharedExternRef),
      kExprLocalGet, 0,
      ...wasmBrOnCastFail(0,
          wasmRefNullType(kWasmExternRef).shared(),
          wasmRefNullType(kWasmExternRef).shared()),
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();
  builder.addFunction('castFailNullToNullExternRef',
    makeSig([sharedExternRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRef, ...wasmEncodeHeapType(sharedExternRef),
      kExprLocalGet, 0,
      ...wasmBrOnCastFail(0,
          wasmRefNullType(kWasmExternRef).shared(),
          wasmRefNullType(kWasmNullExternRef).shared()),
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();

  let instance = builder.instantiate();
  let wasm = instance.exports;
  let obj = new (new SharedStructType(["prop"]));

  assertEquals(0, wasm.castToExternRef(null));
  assertEquals(1, wasm.castToExternRef(undefined));
  assertEquals(1, wasm.castToExternRef(1));
  assertEquals(1, wasm.castToExternRef(obj));

  assertEquals(0, wasm.castToNullExternRef(null));
  assertEquals(0, wasm.castToNullExternRef(undefined));
  assertEquals(0, wasm.castToNullExternRef(1));
  assertEquals(0, wasm.castToNullExternRef(obj));

  assertEquals(1, wasm.castNullToExternRef(null));
  assertEquals(1, wasm.castNullToExternRef(undefined));
  assertEquals(1, wasm.castNullToExternRef(1));
  assertEquals(1, wasm.castNullToExternRef(obj));

  assertEquals(1, wasm.castNullToNullExternRef(null));
  assertEquals(0, wasm.castNullToNullExternRef(undefined));
  assertEquals(0, wasm.castNullToNullExternRef(1));
  assertEquals(0, wasm.castNullToNullExternRef(obj));

  assertEquals(1, wasm.castFailToExternRef(null));
  assertEquals(0, wasm.castFailToExternRef(undefined));
  assertEquals(0, wasm.castFailToExternRef(1));
  assertEquals(0, wasm.castFailToExternRef(obj));

  assertEquals(1, wasm.castFailToNullExternRef(null));
  assertEquals(1, wasm.castFailToNullExternRef(undefined));
  assertEquals(1, wasm.castFailToNullExternRef(1));
  assertEquals(1, wasm.castFailToNullExternRef(obj));

  assertEquals(0, wasm.castFailNullToExternRef(null));
  assertEquals(0, wasm.castFailNullToExternRef(undefined));
  assertEquals(0, wasm.castFailNullToExternRef(1));
  assertEquals(0, wasm.castFailNullToExternRef(obj));

  assertEquals(0, wasm.castFailNullToNullExternRef(null));
  assertEquals(1, wasm.castFailNullToNullExternRef(undefined));
  assertEquals(1, wasm.castFailNullToNullExternRef(1));
  assertEquals(1, wasm.castFailNullToNullExternRef(obj));
})();

(function RefTestAnyRefHierarchy() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let structSuper = builder.addStruct(
    [makeField(kWasmI32, true)], kNoSuperType, false, true);
  let structSub = builder.addStruct(
    [makeField(kWasmI32, true)], structSuper, false, true);
  let array = builder.addArray(kWasmI32, true, kNoSuperType, false, true);

  // Helpers to be able to instantiate a true externref value from wasm.
  let createExternSig = builder.addType(
    makeSig([], [wasmRefNullType(kWasmExternRef).shared()]));
  let createExternIdx = builder.addImport('import', 'createExtern', createExternSig);
  let createExtern = () => undefined;

  let types = {
    any: wasmRefNullType(kWasmAnyRef).shared(),
    eq: wasmRefNullType(kWasmEqRef).shared(),
    struct: wasmRefNullType(kWasmStructRef).shared(),
    anyArray: wasmRefNullType(kWasmArrayRef).shared(),
    array: wasmRefNullType(array),
    structSuper: wasmRefNullType(structSuper),
    structSub: wasmRefNullType(structSub),
    nullref: wasmRefNullType(kWasmNullRef).shared(),
  };

  let createBodies = {
    nullref: [kExprRefNull, ...wasmEncodeHeapType(wasmRefNullType(kWasmNullRef).shared())],
    i31ref: [kExprI32Const, 42, kGCPrefix, kExprRefI31Shared],
    structSuper: [kExprI32Const, 42, kGCPrefix, kExprStructNew, structSuper],
    structSub: [kExprI32Const, 42, kGCPrefix, kExprStructNew, structSub],
    array: [kExprI32Const, 42, kGCPrefix, kExprArrayNewFixed, array, 1],
    any: [kExprCallFunction, createExternIdx, kGCPrefix, kExprAnyConvertExtern],
  };

  // Each Test lists the following:
  // source  => The static type of the source value.
  // values  => All actual values that are subtypes of the static types.
  // targets => A list of types for ref.test. For each type the values are
  //            listed for which ref.test should return 1 (i.e. the ref.test
  //            should succeed).
  // isUpcastOrUnrelated => A subset of the target types which are not legal
  //            targets for br_on_cast which only allows downcasts.)
  let tests = [
    {
      source: 'any',
      values: ['nullref', 'i31ref', 'structSuper', 'structSub', 'array', 'any'],
      targets: {
        any: ['i31ref', 'structSuper', 'structSub', 'array', 'any'],
        eq: ['i31ref', 'structSuper', 'structSub', 'array'],
        struct: ['structSuper', 'structSub'],
        anyArray: ['array'],
        array: ['array'],
        structSuper: ['structSuper', 'structSub'],
        structSub: ['structSub'],
        nullref: [],
      },
      isUpcastOrUnrelated: [],
    },
    {
      source: 'eq',
      values: ['nullref', 'i31ref', 'structSuper', 'structSub', 'array'],
      targets: {
        eq: ['i31ref', 'structSuper', 'structSub', 'array'],
        struct: ['structSuper', 'structSub'],
        anyArray: ['array'],
        array: ['array'],
        structSuper: ['structSuper', 'structSub'],
        structSub: ['structSub'],
        nullref: [],
      },
      isUpcastOrUnrelated: [],
    },
    {
      source: 'struct',
      values: ['nullref', 'structSuper', 'structSub'],
      targets: {
        eq: ['structSuper', 'structSub'],
        struct: ['structSuper', 'structSub'],
        anyArray: ['array'],
        array: ['array'],
        structSuper: ['structSuper', 'structSub'],
        structSub: ['structSub'],
        nullref: [],
      },
      isUpcastOrUnrelated: ['eq', 'anyArray', 'array'],
    },
    {
      source: 'anyArray',
      values: ['nullref', 'array'],
      targets: {
        eq: ['array'],
        struct: [],
        anyArray: ['array'],
        array: ['array'],
        structSuper: [],
        structSub: [],
        nullref: [],
      },
      isUpcastOrUnrelated: ['eq', 'struct', 'structSuper', 'structSub'],
    },
    {
      source: 'structSuper',
      values: ['nullref', 'structSuper', 'structSub'],
      targets: {
        eq: ['structSuper', 'structSub'],
        struct: ['structSuper', 'structSub'],
        anyArray: [],
        array: [],
        structSuper: ['structSuper', 'structSub'],
        structSub: ['structSub'],
        nullref: [],
      },
      isUpcastOrUnrelated: ['eq', 'struct', 'array', 'anyArray'],
    },
  ];

  for (let test of tests) {
    let sourceType = types[test.source];
    // Add creator functions.
    let creatorSig = makeSig([], [sourceType]);
    let creatorType = builder.addType(creatorSig);
    for (let value of test.values) {
      builder.addFunction(`create_${test.source}_${value}`, creatorType)
      .addBody(createBodies[value]).exportFunc();
    }
    // Add ref.test tester functions.
    // The functions take the creator functions as a callback to prevent the
    // compiler to derive the actual type of the value and can only use the
    // static source type.
    for (let target in test.targets) {
      let targetType = types[target];
      let targetTypeNonNullable = wasmRefType(targetType.heap_type);
      if (targetType.is_shared) targetTypeNonNullable.shared();

      builder.addFunction(`test_${test.source}_to_${target}`,
                          makeSig([wasmRefType(creatorType)], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprCallRef, ...wasmUnsignedLeb(creatorType),
        kGCPrefix, kExprRefTest, ...wasmEncodeHeapType(targetType),
      ]).exportFunc();
      builder.addFunction(`test_null_${test.source}_to_${target}`,
                          makeSig([wasmRefType(creatorType)], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprCallRef, ...wasmUnsignedLeb(creatorType),
        kGCPrefix, kExprRefTestNull, ...wasmEncodeHeapType(targetType),
      ]).exportFunc();

      builder.addFunction(`cast_${test.source}_to_${target}`,
                          makeSig([wasmRefType(creatorType)], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprCallRef, ...wasmUnsignedLeb(creatorType),
        kGCPrefix, kExprRefCast, ...wasmEncodeHeapType(targetType),
        kExprRefIsNull, // We can't expose the cast object to JS in most cases.
      ]).exportFunc();

      builder.addFunction(`cast_null_${test.source}_to_${target}`,
                          makeSig([wasmRefType(creatorType)], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprCallRef, ...wasmUnsignedLeb(creatorType),
        kGCPrefix, kExprRefCastNull, ...wasmEncodeHeapType(targetType),
        kExprRefIsNull, // We can't expose the cast object to JS in most cases.
      ]).exportFunc();

      if (test.isUpcastOrUnrelated.includes(target)) {
        // br_on_cast only allows downcasts.
        continue;
      }

      builder.addFunction(`brOnCast_${test.source}_to_${target}`,
                          makeSig([wasmRefType(creatorType)], [kWasmI32]))
      .addBody([
        kExprBlock, kWasmRef, ...wasmEncodeHeapType(targetType),
          kExprLocalGet, 0,
          kExprCallRef, ...wasmUnsignedLeb(creatorType),
          ...wasmBrOnCast(0, sourceType, targetTypeNonNullable),
          kExprI32Const, 0,
          kExprReturn,
        kExprEnd,
        kExprDrop,
        kExprI32Const, 1,
        kExprReturn,
      ])
      .exportFunc();

      builder.addFunction(`brOnCastNull_${test.source}_to_${target}`,
                          makeSig([wasmRefType(creatorType)], [kWasmI32]))
      .addBody([
        kExprBlock, kWasmRefNull, ...wasmEncodeHeapType(targetType),
          kExprLocalGet, 0,
          kExprCallRef, ...wasmUnsignedLeb(creatorType),
          ...wasmBrOnCast(0, sourceType, targetType),
          kExprI32Const, 0,
          kExprReturn,
        kExprEnd,
        kExprDrop,
        kExprI32Const, 1,
        kExprReturn,
      ])
      .exportFunc();

      builder.addFunction(`brOnCastFail_${test.source}_to_${target}`,
                          makeSig([wasmRefType(creatorType)], [kWasmI32]))
      .addBody([
        kExprBlock, kWasmRefNull, ...wasmEncodeHeapType(sourceType),
          kExprLocalGet, 0,
          kExprCallRef, ...wasmUnsignedLeb(creatorType),
          ...wasmBrOnCastFail(0, sourceType, targetTypeNonNullable),
          kExprI32Const, 0,
          kExprReturn,
        kExprEnd,
        kExprDrop,
        kExprI32Const, 1,
        kExprReturn,
      ])
      .exportFunc();

      builder.addFunction(`brOnCastFailNull_${test.source}_to_${target}`,
                          makeSig([wasmRefType(creatorType)], [kWasmI32]))
      .addBody([
        kExprBlock, kWasmRef, ...wasmEncodeHeapType(sourceType),
          kExprLocalGet, 0,
          kExprCallRef, ...wasmUnsignedLeb(creatorType),
          ...wasmBrOnCastFail(0, sourceType, targetType),
          kExprI32Const, 0,
          kExprReturn,
        kExprEnd,
        kExprDrop,
        kExprI32Const, 1,
        kExprReturn,
      ])
      .exportFunc();
    }
  }

  let instance = builder.instantiate({import: {createExtern}});
  let wasm = instance.exports;

  for (let test of tests) {
    for (let [target, validValues] of Object.entries(test.targets)) {
      for (let value of test.values) {
        print(`Test ref.test: ${test.source}(${value}) -> ${target}`);
        let create_value = wasm[`create_${test.source}_${value}`];
        let res = wasm[`test_${test.source}_to_${target}`](create_value);
        assertEquals(validValues.includes(value) ? 1 : 0, res);
        print(`Test ref.test null: ${test.source}(${value}) -> ${target}`);
        res = wasm[`test_null_${test.source}_to_${target}`](create_value);
        assertEquals(
            (validValues.includes(value) || value == "nullref") ? 1 : 0, res);

        print(`Test ref.cast: ${test.source}(${value}) -> ${target}`);
        let cast = wasm[`cast_${test.source}_to_${target}`];
        if (validValues.includes(value)) {
          assertEquals(0, cast(create_value));
        } else {
          assertTraps(kTrapIllegalCast, () => cast(create_value));
        }
        let castNull = wasm[`cast_null_${test.source}_to_${target}`];
        if (validValues.includes(value) || value == "nullref") {
          let expected = value == "nullref" ? 1 : 0;
          assertEquals(expected, castNull(create_value));
        } else {
          assertTraps(kTrapIllegalCast, () => castNull(create_value));
        }

        if (test.isUpcastOrUnrelated.includes(target)) {
          // br_on_cast only allows downcasts.
          continue;
        }

        print(`Test br_on_cast: ${test.source}(${value}) -> ${target}`);
        res = wasm[`brOnCast_${test.source}_to_${target}`](create_value);
        assertEquals(validValues.includes(value) ? 1 : 0, res);
        print(`Test br_on_cast null: ${test.source}(${value}) -> ${target}`);
        res = wasm[`brOnCastNull_${test.source}_to_${target}`](create_value);
        assertEquals(
            validValues.includes(value) || value == "nullref" ? 1 : 0, res);

        print(`Test br_on_cast_fail: ${test.source}(${value}) -> ${target}`);
        res = wasm[`brOnCastFail_${test.source}_to_${target}`](create_value);
        assertEquals(!validValues.includes(value) || value == "nullref" ? 1 : 0, res);
        print(`Test br_on_cast_fail null: ${test.source}(${value}) -> ${target}`);
        res = wasm[`brOnCastFailNull_${test.source}_to_${target}`](create_value);
        assertEquals(!validValues.includes(value) && value != "nullref" ? 1 : 0, res);
      }
    }
  }
})();
