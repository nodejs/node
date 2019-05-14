// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --expose-gc --verify-heap

load("test/mjsunit/wasm/user-properties-common.js");

(function ExportedFunctionTest() {
  print("ExportedFunctionTest");

  print("  instance 1, exporting");
  var builder = new WasmModuleBuilder();
  builder.addFunction("exp", kSig_i_i)
    .addBody([
      kExprGetLocal, 0,
      kExprCallFunction, 0])
    .exportAs("exp");
  let module1 = builder.toModule();
  let instance1 = new WebAssembly.Instance(module1);
  let g = instance1.exports.exp;

  testProperties(g);

  // The WASM-internal fields of {g} are only inspected when {g} is
  // used as an import into another instance.
  print("  instance 2, importing");
  var builder = new WasmModuleBuilder();
  builder.addImport("imp", "func", kSig_i_i);
  let module2 = builder.toModule();
  let instance2 = new WebAssembly.Instance(module2, {imp: {func: g}});

  testProperties(g);
})();
