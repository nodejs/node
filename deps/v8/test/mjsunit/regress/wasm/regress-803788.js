// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-lazy-compilation

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
let q_table = builder.addImportedTable("q", "table")
let q_base = builder.addImportedGlobal("q", "base", kWasmI32);
let q_fun = builder.addImport("q", "fun", kSig_v_v);
builder.addType(kSig_i_ii);
builder.addActiveElementSegment(0, [kExprGlobalGet, q_base], [q_fun]);
let module = new WebAssembly.Module(builder.toBuffer());
let table = new WebAssembly.Table({
  element: "anyfunc",
  initial: 10,
});
let instance = new WebAssembly.Instance(module, {
  q: {
    base: 0,
    table: table,
    fun: () => (0)
  }
});
