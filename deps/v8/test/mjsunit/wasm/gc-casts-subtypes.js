// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc --experimental-wasm-type-reflection
// Flags: --no-wasm-gc-structref-as-dataref

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Test casting null from one type to another using ref.test & ref.cast.
(function RefCastFromNull() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let structSuper = builder.addStruct([makeField(kWasmI32, true)]);
  let structSub = builder.addStruct([makeField(kWasmI32, true)], structSuper);
  let array = builder.addArray(kWasmI32);

  // Note: Casting between unrelated types is allowed as long as the types
  // belong to the same type hierarchy (func / any / extern). In these cases the
  // check will always fail.
  let tests = [
    [kWasmAnyRef, kWasmAnyRef, 'AnyToAny'],
    [kWasmFuncRef, kWasmFuncRef, 'FuncToFunc'],
    [kWasmExternRef, kWasmExternRef, 'ExternToExtern'],
    [kWasmNullFuncRef, kWasmNullFuncRef, 'NullFuncToNullFunc'],
    [kWasmNullExternRef, kWasmNullExternRef, 'NullExternToNullExtern'],
    [structSub, array, 'StructToArray'],
    [kWasmFuncRef, kWasmNullFuncRef, 'FuncToNullFunc'],
    [kWasmNullFuncRef, kWasmFuncRef, 'NullFuncToFunc'],
    [kWasmExternRef, kWasmNullExternRef, 'ExternToNullExtern'],
    [kWasmNullExternRef, kWasmExternRef, 'NullExternToExtern'],
    [kWasmNullRef, kWasmAnyRef, 'NullToAny'],
    [kWasmI31Ref, structSub, 'I31ToStruct'],
    [kWasmEqRef, kWasmI31Ref, 'EqToI31'],
    [structSuper, structSub, 'StructSuperToStructSub'],
    [structSub, structSuper, 'StructSubToStructSuper'],
  ];

  for (let [sourceType, targetType, testName] of tests) {
    builder.addFunction('testNull' + testName, makeSig([], [kWasmI32]))
    .addLocals(wasmRefNullType(sourceType), 1)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefTest, targetType & kLeb128Mask,
    ]).exportFunc();
    builder.addFunction('castNull' + testName, makeSig([], []))
    .addLocals(wasmRefNullType(sourceType), 1)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefCast, targetType & kLeb128Mask,
      kExprDrop,
    ]).exportFunc();
  }

  let instance = builder.instantiate();
  let wasm = instance.exports;

  for (let [sourceType, targetType, testName] of tests) {
    assertEquals(0, wasm['testNull' + testName]());
    assertTraps(kTrapIllegalCast, wasm['castNull' + testName]);
  }
})();

(function RefTestFuncRef() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sigSuper = builder.addType(makeSig([kWasmI32], []));
  let sigSub = builder.addType(makeSig([kWasmI32], []), sigSuper);

  builder.addFunction('fctSuper', sigSuper).addBody([]).exportFunc();
  builder.addFunction('fctSub', sigSub).addBody([]).exportFunc();
  builder.addFunction('testFromFuncRef',
      makeSig([kWasmFuncRef], [kWasmI32, kWasmI32, kWasmI32, kWasmI32]))
    .addBody([
      kExprLocalGet, 0, kGCPrefix, kExprRefTest, kFuncRefCode,
      kExprLocalGet, 0, kGCPrefix, kExprRefTest, kNullFuncRefCode,
      kExprLocalGet, 0, kGCPrefix, kExprRefTest, sigSuper,
      kExprLocalGet, 0, kGCPrefix, kExprRefTest, sigSub,
    ]).exportFunc();

  let instance = builder.instantiate();
  let wasm = instance.exports;
  let jsFct = new WebAssembly.Function(
      {parameters:['i32', 'i32'], results: ['i32']},
      function mul(a, b) { return a * b; });
  assertEquals([1, 0, 0, 0], wasm.testFromFuncRef(jsFct));
  assertEquals([1, 0, 1, 0], wasm.testFromFuncRef(wasm.fctSuper));
  assertEquals([1, 0, 1, 1], wasm.testFromFuncRef(wasm.fctSub));
})();

(function RefCastFuncRef() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sigSuper = builder.addType(makeSig([kWasmI32], []));
  let sigSub = builder.addType(makeSig([kWasmI32], []), sigSuper);

  builder.addFunction('fctSuper', sigSuper).addBody([]).exportFunc();
  builder.addFunction('fctSub', sigSub).addBody([]).exportFunc();
  builder.addFunction('castToFuncRef', makeSig([kWasmFuncRef], [kWasmFuncRef]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCast, kFuncRefCode])
    .exportFunc();
  builder.addFunction('castToNullFuncRef',
                      makeSig([kWasmFuncRef], [kWasmFuncRef]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCast, kNullFuncRefCode])
    .exportFunc();
  builder.addFunction('castToSuper', makeSig([kWasmFuncRef], [kWasmFuncRef]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCast, sigSuper])
    .exportFunc();
  builder.addFunction('castToSub', makeSig([kWasmFuncRef], [kWasmFuncRef]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCast, sigSub])
    .exportFunc();

  let instance = builder.instantiate();
  let wasm = instance.exports;
  let jsFct = new WebAssembly.Function(
      {parameters:['i32', 'i32'], results: ['i32']},
      function mul(a, b) { return a * b; });
  assertSame(jsFct, wasm.castToFuncRef(jsFct));
  assertSame(wasm.fctSuper, wasm.castToFuncRef(wasm.fctSuper));
  assertSame(wasm.fctSub, wasm.castToFuncRef(wasm.fctSub));

  assertTraps(kTrapIllegalCast, () => wasm.castToNullFuncRef(jsFct));
  assertTraps(kTrapIllegalCast, () => wasm.castToNullFuncRef(wasm.fctSuper));
  assertTraps(kTrapIllegalCast, () => wasm.castToNullFuncRef(wasm.fctSub));

  assertTraps(kTrapIllegalCast, () => wasm.castToSuper(jsFct));
  assertSame(wasm.fctSuper, wasm.castToSuper(wasm.fctSuper));
  assertSame(wasm.fctSub, wasm.castToSuper(wasm.fctSub));

  assertTraps(kTrapIllegalCast, () => wasm.castToSub(jsFct));
  assertTraps(kTrapIllegalCast, () => wasm.castToSub(wasm.fctSuper));
  assertSame(wasm.fctSub, wasm.castToSub(wasm.fctSub));
})();

(function RefTestExternRef() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  builder.addFunction('testExternRef',
      makeSig([kWasmExternRef], [kWasmI32, kWasmI32]))
    .addBody([
      kExprLocalGet, 0, kGCPrefix, kExprRefTest, kExternRefCode,
      kExprLocalGet, 0, kGCPrefix, kExprRefTest, kNullExternRefCode,
    ]).exportFunc();

  let instance = builder.instantiate();
  let wasm = instance.exports;
  assertEquals([0, 0], wasm.testExternRef(null));
  assertEquals([1, 0], wasm.testExternRef(undefined));
  assertEquals([1, 0], wasm.testExternRef(1));
  assertEquals([1, 0], wasm.testExternRef({}));
  assertEquals([1, 0], wasm.testExternRef(wasm.testExternRef));
})();

(function RefCastExternRef() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  builder.addFunction('castToExternRef',
      makeSig([kWasmExternRef], [kWasmExternRef]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCast, kExternRefCode])
    .exportFunc();
  builder.addFunction('castToNullExternRef',
    makeSig([kWasmExternRef], [kWasmExternRef]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCast, kNullExternRefCode])
  .exportFunc();

  let instance = builder.instantiate();
  let wasm = instance.exports;
  assertTraps(kTrapIllegalCast, () => wasm.castToExternRef(null));
  assertEquals(undefined, wasm.castToExternRef(undefined));
  assertEquals(1, wasm.castToExternRef(1));
  let obj = {};
  assertSame(obj, wasm.castToExternRef(obj));
  assertSame(wasm.castToExternRef, wasm.castToExternRef(wasm.castToExternRef));

  assertTraps(kTrapIllegalCast, () => wasm.castToNullExternRef(null));
  assertTraps(kTrapIllegalCast, () => wasm.castToNullExternRef(undefined));
  assertTraps(kTrapIllegalCast, () => wasm.castToNullExternRef(1));
  assertTraps(kTrapIllegalCast, () => wasm.castToNullExternRef(obj));
  assertTraps(kTrapIllegalCast,
              () => wasm.castToNullExternRef(wasm.castToExternRef));
})();

(function RefTestAnyRefHierarchy() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let structSuper = builder.addStruct([makeField(kWasmI32, true)]);
  let structSub = builder.addStruct([makeField(kWasmI32, true)], structSuper);
  let array = builder.addArray(kWasmI32);

  let types = {
    any: kWasmAnyRef,
    eq: kWasmEqRef,
    struct: kWasmStructRef,
    anyArray: kWasmArrayRef,
    array: wasmRefNullType(array),
    structSuper: wasmRefNullType(structSuper),
    structSub: wasmRefNullType(structSub),
  };

  let createBodies = {
    nullref: [kExprRefNull, kNullRefCode],
    i31ref: [kExprI32Const, 42, kGCPrefix, kExprI31New],
    structSuper: [kExprI32Const, 42, kGCPrefix, kExprStructNew, structSuper],
    structSub: [kExprI32Const, 42, kGCPrefix, kExprStructNew, structSub],
    array: [kExprI32Const, 42, kGCPrefix, kExprArrayNewFixed, array, 1],
  };

  // Each Test lists the following:
  // source  => The static type of the source value.
  // values  => All actual values that are subtypes of the static types.
  // targets => A list of types for ref.test. For each type the values are
  //            listed for which ref.test should return 1 (i.e. the ref.test
  //            should succeed).
  let tests = [
    {
      source: 'any',
      values: ['nullref', 'i31ref', 'structSuper', 'structSub', 'array'],
      targets: {
        any: ['i31ref', 'structSuper', 'structSub', 'array'],
        eq: ['i31ref', 'structSuper', 'structSub', 'array'],
        struct: ['structSuper', 'structSub'],
        anyArray: ['array'],
        array: ['array'],
        structSuper: ['structSuper', 'structSub'],
        structSub: ['structSub'],
      }
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
      }
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
      }
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
      }
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
      }
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
      // Get heap type for concrete types or apply Leb128 mask on the abstract
      // type.
      let heapType = types[target].heap_type ?? (types[target] & kLeb128Mask);
      builder.addFunction(`test_${test.source}_to_${target}`,
                          makeSig([wasmRefType(creatorType)], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprCallRef, creatorType,
        kGCPrefix, kExprRefTest, heapType,
      ]).exportFunc();
      builder.addFunction(`test_null_${test.source}_to_${target}`,
                          makeSig([wasmRefType(creatorType)], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprCallRef, creatorType,
        kGCPrefix, kExprRefTestNull, heapType,
      ]).exportFunc();

      builder.addFunction(`cast_${test.source}_to_${target}`,
                          makeSig([wasmRefType(creatorType)], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprCallRef, creatorType,
        kGCPrefix, kExprRefCast, heapType,
        kExprRefIsNull, // We can't expose the cast object to JS in most cases.
      ]).exportFunc();

      builder.addFunction(`cast_null_${test.source}_to_${target}`,
                          makeSig([wasmRefType(creatorType)], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprCallRef, creatorType,
        kGCPrefix, kExprRefCastNull, heapType,
        kExprRefIsNull, // We can't expose the cast object to JS in most cases.
      ]).exportFunc();
    }
  }

  let instance = builder.instantiate();
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
      }
    }
  }
})();
