// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

var builder = new WasmModuleBuilder();
var $array0 = builder.addArray(kWasmI64, true);
var $glob2 = builder.addGlobal(wasmRefType($array0), false, false, [
  ...wasmI32Const(-1),  // length
  kGCPrefix, kExprArrayNewDefault, $array0
]);
builder.addFunction("foo", kSig_l_i).exportFunc().addBody([
  kExprGlobalGet, $glob2.index,
  kExprI32Const, 0,
  kGCPrefix, kExprArrayGet, $array0,
]);

assertThrows(() => builder.instantiate(), WebAssembly.RuntimeError,
             /requested new array is too large/);

this.console.profile();  // Does not crash.
