// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load("test/mjsunit/wasm/wasm-module-builder.js");

function createExport(fun) {
  let builder = new WasmModuleBuilder();
  let fun_index = builder.addImport("m", "fun", kSig_i_v)
  builder.addExport("fun", fun_index);
  let instance = builder.instantiate({ m: { fun: fun }});
  return instance.exports.fun;
}

// Test that re-exporting a generic JavaScript function changes identity, as
// the resulting export is an instance of {WebAssembly.Function} instead.
(function TestReExportOfJS() {
  print(arguments.callee.name);
  function fun() { return 7 }
  let exported = createExport(fun);
  assertNotSame(exported, fun);
  assertEquals(7, exported());
  assertEquals(7, fun());
})();

// Test that re-exporting and existing {WebAssembly.Function} that represents
// regular WebAssembly functions preserves identity.
(function TestReReExportOfWasm() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction('fun', kSig_i_v).addBody([kExprI32Const, 9]).exportFunc();
  let fun = builder.instantiate().exports.fun;
  let exported = createExport(fun);
  assertSame(exported, fun);
  assertEquals(9, fun());
})();

// Test that re-exporting and existing {WebAssembly.Function} that represents
// generic JavaScript functions preserves identity.
(function TestReReExportOfJS() {
  print(arguments.callee.name);
  let fun = createExport(() => 11)
  let exported = createExport(fun);
  assertSame(exported, fun);
  assertEquals(11, fun());
})();
