// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-max-table-size=10

load("test/mjsunit/wasm/wasm-module-builder.js");

// With the flags we set the maximum table size to 10, so 11 is out-of-bounds.
const oob = 11;

(function TestJSTableInitialAboveTheLimit() {
  print(arguments.callee.name);
  assertThrows(
    () => new WebAssembly.Table({ initial: oob, element: "anyfunc" }),
    RangeError, /above the upper bound/);
})();

(function TestJSTableMaximumAboveTheLimit() {
  print(arguments.callee.name);
  assertThrows(
    () => new WebAssembly.Table({ initial: 1, maximum: oob, element: "anyfunc" }),
    RangeError, /above the upper bound/);
})();

(function TestDecodeTableInitialAboveTheLimit() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  builder.setTableBounds(oob);
  assertThrows(
    () => builder.instantiate(),
    WebAssembly.CompileError, /is larger than implementation limit/);
})();

(function TestDecodeTableMaximumAboveTheLimit() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  builder.setTableBounds(1, oob);
  assertThrows(
    () => builder.instantiate(),
    WebAssembly.CompileError, /is larger than implementation limit/);
})();
