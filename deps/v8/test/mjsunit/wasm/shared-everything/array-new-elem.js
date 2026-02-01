// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestArrayNewElemShared() {
  let builder = new WasmModuleBuilder();
  let sharedStruct = builder.addStruct(
    {fields: [makeField(kWasmI32, false)], shared: true})
  let sharedArray = builder.addArray(
    wasmRefNullType(sharedStruct), true, kNoSuperType, false, true);
  let segment = builder.addPassiveElementSegment(
    [[kGCPrefix, kExprStructNewDefault, sharedStruct]],
    wasmRefNullType(sharedStruct));

  builder.addFunction('createArray',
    makeSig([kWasmI32], [wasmRefType(sharedArray)]))
  .addBody([
    kExprI32Const, 0,
    kExprLocalGet, 0,
    kGCPrefix, kExprArrayNewElem, sharedArray, segment,
  ]).exportFunc();

  builder.addFunction('getElement',
    makeSig([wasmRefType(sharedArray), kWasmI32],
            [wasmRefNullType(sharedStruct)]))
  .addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kGCPrefix, kExprArrayGet, sharedArray,
  ]).exportFunc();

  const wasm1 = builder.instantiate().exports;
  let array10 = wasm1.createArray(1);
  let array11 = wasm1.createArray(1);
  const wasm2 = builder.instantiate().exports;
  let array20 = wasm2.createArray(1);

  assertSame(wasm1.getElement(array10, 0), wasm1.getElement(array11, 0));
  assertNotSame(wasm1.getElement(array10, 0), wasm1.getElement(array20, 0));
})();
