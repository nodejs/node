// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-stringref --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();

let impDataView = builder.addImport(
    'm', 'getInt32', makeSig([kWasmExternRef, kWasmI32, kWasmI32], [kWasmI32]));
let impIndexOf = builder.addImport(
    'm', 'indexOf', makeSig([kWasmStringRef, kWasmStringRef, kWasmI32], [kWasmI32]));
let impToLowerCase = builder.addImport(
    'm', 'toLowerCase', makeSig([kWasmStringRef], [kWasmStringRef]));
let impIntToString = builder.addImport(
    'm', 'intToString', makeSig([kWasmI32, kWasmI32], [kWasmStringRef]));

builder.addFunction('getInt32', makeSig([kWasmExternRef, kWasmI32], [kWasmI32]))
.exportFunc()
.addBody([
  kExprTry, kWasmI32,
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprI32Const, 0,
    kExprCallFunction, impDataView,
  kExprCatchAll,
    ...wasmI32Const(-1),
  kExprEnd,
]);

builder.addFunction('testIndexOf', makeSig([kWasmStringRef, kWasmStringRef, kWasmI32], [kWasmI32]))
.exportFunc()
.addBody([
  kExprTry, kWasmI32,
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprLocalGet, 2,
    kExprCallFunction, impIndexOf,
  kExprCatchAll,
    ...wasmI32Const(-1),
  kExprEnd,
]);

builder.addFunction('testToLowerCase', makeSig([kWasmStringRef], [kWasmStringRef]))
.exportFunc()
.addBody([
  kExprTry, kStringRefCode,
    kExprLocalGet, 0,
    kExprCallFunction, impToLowerCase,
  kExprCatchAll,
    kExprRefNull, kStringRefCode,
  kExprEnd,
]);

builder.addFunction('testIntToString', makeSig([kWasmI32, kWasmI32], [kWasmStringRef]))
.exportFunc()
.addBody([
  kExprTry, kStringRefCode,
    kExprLocalGet, 0, kExprLocalGet, 1,
    kExprCallFunction, impIntToString,
  kExprCatchAll,
    kExprRefNull, kStringRefCode,
  kExprEnd,
]);

let wasm = builder.instantiate({
  m: {
    getInt32: Function.prototype.call.bind(DataView.prototype.getInt32),
    indexOf: Function.prototype.call.bind(String.prototype.indexOf),
    toLowerCase: Function.prototype.call.bind(String.prototype.toLowerCase),
    intToString: Function.prototype.call.bind(Number.prototype.toString),
  }
}).exports;

runTest();
// Explicitly tier up all exported Wasm functions and repeat test with turbofan.
for (let wasmFct in wasm) {
  %WasmTierUpFunction(wasm[wasmFct]);
}
runTest();

function runTest() {
  // DataView tests.
  let arrayBuffer = new ArrayBuffer(8);
  let dataview = new DataView(arrayBuffer);
  dataview.setInt32(4, 42);

  assertEquals(42, wasm.getInt32(dataview, 4));
  assertEquals(0, wasm.getInt32(dataview, 0));
  // Test out-of-bounds accesses.
  assertEquals(-1, wasm.getInt32(dataview, 5));
  assertEquals(-1, wasm.getInt32(dataview, 7));
  assertEquals(-1, wasm.getInt32(dataview, 8));
  assertEquals(-1, wasm.getInt32(dataview, 9));
  // Test type error.
  assertEquals(-1, wasm.getInt32(arrayBuffer, 4));
  assertEquals(-1, wasm.getInt32(null, 4));
  assertEquals(-1, wasm.getInt32(undefined, 4));
  // Test detached.
  arrayBuffer.transfer();
  assertEquals(-1, wasm.getInt32(dataview, 4));
  assertEquals(-1, wasm.getInt32(dataview, 0));

  // String.indexOf tests.
  assertEquals(-1, wasm.testIndexOf(null, "a", 0));
  assertEquals(1, wasm.testIndexOf("abc", "b", 0));

  // String.toLowerCase tests.
  assertEquals(null, wasm.testToLowerCase(null));
  assertEquals("abc", wasm.testToLowerCase("ABC"));

  // IntToString tests.
  assertEquals(null, wasm.testIntToString(42, 37));
  assertEquals("42", wasm.testIntToString(42, 10));
}
