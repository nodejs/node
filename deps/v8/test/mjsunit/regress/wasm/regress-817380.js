// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-lazy-compilation

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

const builder1 = new WasmModuleBuilder();
builder1.addFunction('mul', kSig_i_ii)
    .addBody([kExprGetLocal, 0, kExprGetLocal, 1, kExprI32Mul])
    .exportFunc();
const mul = builder1.instantiate().exports.mul;
const table = new WebAssembly.Table({
  element: 'anyfunc',
  initial: 10,
});
const builder2 = new WasmModuleBuilder();
const mul_import = builder2.addImport('q', 'wasm_mul', kSig_i_ii);
builder2.addImportedTable('q', 'table');
const glob_import = builder2.addImportedGlobal('q', 'glob', kWasmI32);
builder2.addFunctionTableInit(glob_import, true, [mul_import]);
builder2.instantiate(
    {q: {glob: 0, js_div: i => i, wasm_mul: mul, table: table}});
