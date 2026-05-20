// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared --no-wasm-lazy-validation

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function AnyConvertExternValid() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("main",
    makeSig([wasmRefNullType(kWasmExternRef).shared()],
            [wasmRefNullType(kWasmAnyRef).shared()]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprAnyConvertExtern])
  .exportFunc();

  let wasm = builder.instantiate().exports;
  assertEquals(null, wasm.main(null));
  assertEquals(1, wasm.main(1));
  assertEquals("a", wasm.main("a"));
})();

(function AnyConvertExternInvalid() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("main",
    makeSig([wasmRefNullType(kWasmExternRef).shared()],
            [wasmRefNullType(kWasmAnyRef)]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprAnyConvertExtern])
  .exportFunc();
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
    /expected anyref, got shared anyref/);

  builder = new WasmModuleBuilder();
  builder.addFunction("main",
    makeSig([wasmRefNullType(kWasmExternRef)],
            [wasmRefNullType(kWasmAnyRef).shared()]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprAnyConvertExtern])
  .exportFunc();
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
    /expected shared anyref, got anyref/);
})();

(function ExternConvertAnyValid() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("main",
    makeSig([wasmRefNullType(kWasmAnyRef).shared()],
            [wasmRefNullType(kWasmExternRef).shared()]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprExternConvertAny])
  .exportFunc();

  let wasm = builder.instantiate().exports;
  assertEquals(null, wasm.main(null));
  assertEquals(1, wasm.main(1));
  assertEquals("a", wasm.main("a"));
})();

(function ExternConvertAnyInvalid() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("main",
    makeSig([wasmRefNullType(kWasmAnyRef).shared()],
            [wasmRefNullType(kWasmExternRef)]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprExternConvertAny])
  .exportFunc();
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
    /expected externref, got shared externref/);

  builder = new WasmModuleBuilder();
  builder.addFunction("main",
    makeSig([wasmRefNullType(kWasmAnyRef)],
            [wasmRefNullType(kWasmExternRef).shared()]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprExternConvertAny])
  .exportFunc();
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
    /expected shared externref, got externref/);
})();

(function RefEqValid() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sharedArray = builder.addArray(kWasmI32, true, kNoSuperType, false, true);
  let sharedStruct = builder.addStruct(
    [makeField(kWasmI32, true)], kNoSuperType, false, true);

  let types = [
    [
      "eq",
      wasmRefNullType(kWasmEqRef).shared(),
      wasmRefNullType(kWasmEqRef).shared(),
    ], [
      "array",
      wasmRefNullType(kWasmArrayRef).shared(),
      wasmRefNullType(kWasmArrayRef).shared(),
    ], [
      "struct",
      wasmRefNullType(kWasmStructRef).shared(),
      wasmRefNullType(kWasmStructRef).shared(),
    ], [
      "none",
      wasmRefNullType(kWasmNullRef).shared(),
      wasmRefNullType(kWasmNullRef).shared(),
    ], [
      "eq_array",
      wasmRefNullType(kWasmEqRef).shared(),
      wasmRefNullType(kWasmArrayRef).shared(),
    ], [
      "sharedArray_eq",
      wasmRefNullType(sharedArray),
      wasmRefNullType(kWasmEqRef).shared(),
    ], [
      "sharedArray_sharedStruct",
      wasmRefNullType(sharedArray),
      wasmRefNullType(sharedStruct),
    ],
  ];

  for (let [name, type1, type2] of types) {
    builder.addFunction(name, makeSig([type1, type2], [kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprRefEq,
    ])
    .exportFunc();
  }

  builder.instantiate();
})();

(function ArrayLenShared() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sharedArray = builder.addArray(kWasmI32, true, kNoSuperType, false, true);
  builder.addFunction("main",
    makeSig([kWasmI32], [kWasmI32]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprArrayNewDefault, sharedArray,
    kGCPrefix, kExprArrayLen,
  ])
  .exportFunc();

  let wasm = builder.instantiate().exports;
  assertEquals(1, wasm.main(1));
  assertEquals(12, wasm.main(12));
})();

(function I31Valid() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("create",
    makeSig([kWasmI32], [wasmRefType(kWasmI31Ref).shared()]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprRefI31Shared,
  ])
  .exportFunc();

  builder.addFunction("getNullS",
    makeSig([wasmRefNullType(kWasmI31Ref).shared()], [kWasmI32]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprI31GetS,
  ])
  .exportFunc();

  builder.addFunction("getNullU",
    makeSig([wasmRefNullType(kWasmI31Ref).shared()], [kWasmI32]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprI31GetU,
  ])
  .exportFunc();

  builder.addFunction("getS",
    makeSig([wasmRefType(kWasmI31Ref).shared()], [kWasmI32]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprI31GetS,
  ])
  .exportFunc();

  builder.addFunction("getU",
    makeSig([wasmRefType(kWasmI31Ref).shared()], [kWasmI32]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprI31GetU,
  ])
  .exportFunc();

  let wasm = builder.instantiate().exports;
  assertEquals(0, wasm.create(0));
  assertEquals(1, wasm.create(1));
  assertEquals(-12, wasm.create(-12));
  assertEquals(42, wasm.getNullS(42));
  assertEquals(-127, wasm.getNullS(-127));
  assertEquals(42, wasm.getS(42));
  assertEquals(-127, wasm.getS(-127));
  assertEquals(42, wasm.getNullU(42));
  assertEquals(0x7FFF_FFFF, wasm.getNullU(-1));
  assertEquals(42, wasm.getU(42));
  assertEquals(0x7FFF_FFFF, wasm.getU(-1));
  assertTraps(kTrapNullDereference, () => wasm.getNullS(null));
  assertTraps(kTrapNullDereference, () => wasm.getNullU(null));
  // Nothing to do with the i31.get_s / get_u, null check done by the wrapper:
  assertThrows(() => wasm.getS(null));
  assertThrows(() => wasm.getU(null));
})();

(function I31Invalid() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("main",
    makeSig([kWasmI32], [wasmRefNullType(kWasmI31Ref)]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprRefI31Shared,
  ])
  .exportFunc();
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
    /expected i31ref, got \(ref shared i31\)/);

  builder = new WasmModuleBuilder();
  builder.addFunction("main",
    makeSig([kWasmI32], [wasmRefNullType(kWasmI31Ref).shared()]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprRefI31,
  ])
  .exportFunc();
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
    /expected shared i31ref, got \(ref i31\)/);
})();
