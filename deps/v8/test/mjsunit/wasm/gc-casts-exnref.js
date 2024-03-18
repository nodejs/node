// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-exnref --no-experimental-wasm-inlining

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let getExnRef = function() {
  let tag = new WebAssembly.Tag({parameters:[]});
  return new WebAssembly.Exception(tag, []);
};

(function RefTestExnRef() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let tag = builder.addTag(makeSig([], []));

  builder.addFunction('testExnRef',
      makeSig([kWasmExnRef], [kWasmI32, kWasmI32]))
    .addBody([
      kExprLocalGet, 0, kGCPrefix, kExprRefTest, kExnRefCode,
      kExprLocalGet, 0, kGCPrefix, kExprRefTest, kNullExnRefCode,
    ]).exportFunc();

  builder.addFunction('testNullExnRef',
      makeSig([kWasmExnRef], [kWasmI32, kWasmI32]))
    .addBody([
      kExprLocalGet, 0, kGCPrefix, kExprRefTestNull, kExnRefCode,
      kExprLocalGet, 0, kGCPrefix, kExprRefTestNull, kNullExnRefCode,
    ]).exportFunc();

  let instance = builder.instantiate();
  let wasm = instance.exports;
  assertEquals([0, 0], wasm.testExnRef(null));
  assertEquals([1, 0], wasm.testExnRef(getExnRef()));
  assertEquals([1, 1], wasm.testNullExnRef(null));
  assertEquals([1, 0], wasm.testNullExnRef(getExnRef()));
})();

(function RefCastExnRef() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  builder.addFunction('castToExnRef',
      makeSig([kWasmExnRef], [kWasmExnRef]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCast, kExnRefCode])
    .exportFunc();
  builder.addFunction('castToNullExnRef',
    makeSig([kWasmExnRef], [kWasmExnRef]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCast, kNullExnRefCode])
  .exportFunc();
  builder.addFunction('castNullToExnRef',
    makeSig([kWasmExnRef], [kWasmExnRef]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCastNull, kExnRefCode])
  .exportFunc();
  builder.addFunction('castNullToNullExnRef',
    makeSig([kWasmExnRef], [kWasmExnRef]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCastNull, kNullExnRefCode])
  .exportFunc();

  let instance = builder.instantiate();
  let wasm = instance.exports;

  let exnRef = getExnRef();
  assertTraps(kTrapIllegalCast, () => wasm.castToExnRef(null));
  assertEquals(exnRef, wasm.castToExnRef(exnRef));
  assertTraps(kTrapIllegalCast, () => wasm.castToNullExnRef(null));
  assertTraps(kTrapIllegalCast, () => wasm.castToNullExnRef(exnRef));

  assertSame(null, wasm.castNullToExnRef(null));
  assertEquals(exnRef, wasm.castNullToExnRef(exnRef));
  assertSame(null, wasm.castNullToNullExnRef(null));
  assertTraps(kTrapIllegalCast, () => wasm.castNullToNullExnRef(exnRef));
})();

(function BrOnCastExnRef() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  builder.addFunction('castToExnRef',
    makeSig([kWasmExnRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRef, kExnRefCode,
      kExprLocalGet, 0,
      ...wasmBrOnCast(
          0, wasmRefNullType(kWasmExnRef), wasmRefType(kWasmExnRef)),
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();
  builder.addFunction('castToNullExnRef',
    makeSig([kWasmExnRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRef, kNullExnRefCode,
      kExprLocalGet, 0,
      ...wasmBrOnCast(
          0, wasmRefNullType(kWasmExnRef), wasmRefType(kWasmNullExnRef)),
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();

  builder.addFunction('castNullToExnRef',
    makeSig([kWasmExnRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRefNull, kExnRefCode,
      kExprLocalGet, 0,
      ...wasmBrOnCast(0,
          wasmRefNullType(kWasmExnRef), wasmRefNullType(kWasmExnRef)),
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();
  builder.addFunction('castNullToNullExnRef',
    makeSig([kWasmExnRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRefNull, kNullExnRefCode,
      kExprLocalGet, 0,
      ...wasmBrOnCast(0,
          wasmRefNullType(kWasmExnRef), wasmRefNullType(kWasmNullExnRef)),
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();

  builder.addFunction('castFailToExnRef',
    makeSig([kWasmExnRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRefNull, kExnRefCode,
      kExprLocalGet, 0,
      ...wasmBrOnCastFail(0,
          wasmRefNullType(kWasmExnRef), wasmRefType(kWasmExnRef)),
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();
  builder.addFunction('castFailToNullExnRef',
    makeSig([kWasmExnRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRefNull, kExnRefCode,
      kExprLocalGet, 0,
      ...wasmBrOnCastFail(0,
          wasmRefNullType(kWasmExnRef), wasmRefType(kWasmNullExnRef)),
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();

  builder.addFunction('castFailNullToExnRef',
    makeSig([kWasmExnRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRef, kExnRefCode,
      kExprLocalGet, 0,
      ...wasmBrOnCastFail(0,
          wasmRefNullType(kWasmExnRef), wasmRefNullType(kWasmExnRef)),
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();
  builder.addFunction('castFailNullToNullExnRef',
    makeSig([kWasmExnRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRef, kExnRefCode,
      kExprLocalGet, 0,
      ...wasmBrOnCastFail(0,
          wasmRefNullType(kWasmExnRef), wasmRefNullType(kWasmNullExnRef)),
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
  let exnRef = getExnRef();

  assertEquals(0, wasm.castToExnRef(null));
  assertEquals(1, wasm.castToExnRef(exnRef));

  assertEquals(0, wasm.castToNullExnRef(null));
  assertEquals(0, wasm.castToNullExnRef(exnRef));

  assertEquals(1, wasm.castNullToExnRef(null));
  assertEquals(1, wasm.castNullToExnRef(exnRef));

  assertEquals(1, wasm.castNullToNullExnRef(null));
  assertEquals(0, wasm.castNullToNullExnRef(exnRef));

  assertEquals(1, wasm.castFailToExnRef(null));
  assertEquals(0, wasm.castFailToExnRef(exnRef));

  assertEquals(1, wasm.castFailToNullExnRef(null));
  assertEquals(1, wasm.castFailToNullExnRef(exnRef));

  assertEquals(0, wasm.castFailNullToExnRef(null));
  assertEquals(0, wasm.castFailNullToExnRef(exnRef));

  assertEquals(0, wasm.castFailNullToNullExnRef(null));
  assertEquals(1, wasm.castFailNullToNullExnRef(exnRef));
})();
