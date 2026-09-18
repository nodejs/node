// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

if (typeof WebAssembly !== 'undefined') {
  d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

  let builder = new WasmModuleBuilder();

  let $t = builder.addTable(kWasmNullExternRef, 0, 1).exportAs('table');

  builder.addFunction('main', makeSig([], [kWasmI32]))
  .addLocals(kWasmExternRef, 1)
  .addBody([
    ...wasmI32Const(0),
    kExprTableGet, $t.index,
    kExprRefIsNull,

    // Because our "feature detection" is very flawed, we need some `kGCPrefix`
    // instruction to get wasm gc optimization.
    kExprI32Const, 0,
    kGCPrefix, kExprRefI31,
    kExprDrop,
  ]).exportFunc();

  let wasm = builder.instantiate().exports;

  wasm.table.grow(1);
  assertEquals(1, wasm.main());
  %WasmTierUpFunction(wasm.main);
  assertEquals(1, wasm.main());
}
