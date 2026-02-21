// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jit-fuzzing --experimental-wasm-type-reflection

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const funcVoid = new WebAssembly.Function({
  parameters: [],
  results: []
}, arg => null);

const funcI32 = new WebAssembly.Function({
  parameters: [],
  results: ["i32"]
}, funcVoid);

(function TestCallImport() {
  let builder = new WasmModuleBuilder();
  let importedFunc = builder.addImport("", "importedFunc", kSig_i_v);
  builder.addFunction("main", kSig_i_v).addBody([
      kExprCallFunction, importedFunc
  ]).exportFunc();
  let instance = builder.instantiate({"": {importedFunc: funcI32}});
  instance.exports.main();
})();

(function TestCallIndirect() {
  const table = new WebAssembly.Table({
    element: "anyfunc",
    initial: 2,
  });
  table.set(1, funcI32);

  let builder = new WasmModuleBuilder();
  builder.addImportedTable('m', 't', 1);
  let sig = builder.addType(kSig_i_v);
  builder.addFunction("main", kSig_v_v).addBody([
      kExprI32Const, 1,
      kExprCallIndirect, sig, kTableZero,
      kExprDrop
  ]).exportFunc();
  let instance = builder.instantiate({m: {t: table}});

  for (let i = 0;  i < 3; ++i) {
    instance.exports.main();
  }
})();
