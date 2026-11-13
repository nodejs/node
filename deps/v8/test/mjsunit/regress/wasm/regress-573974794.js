// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors --wasm-merged-descriptors
// Flags: --wasm-wrapper-tiering-budget=1

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.startRecGroup();
let $desc = builder.nextTypeIndex() + 1;
let $struct = builder.addStruct({descriptor: $desc});
builder.addStruct({describes: $struct});
builder.endRecGroup();

let $g_desc = builder.addGlobal(
    wasmRefType($desc).exact(), false, false,
    [kGCPrefix, kExprStructNewDefault, $desc]);

builder.addFunction('get_desc', makeSig([], [kWasmAnyRef]))
    .exportFunc()
    .addBody([kExprGlobalGet, $g_desc.index]);

builder.addFunction('take_any', makeSig([kWasmAnyRef], [kWasmI32]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefTest, kStructRefCode,
    ]);

builder.addFunction('take_eq', makeSig([kWasmEqRef], [kWasmI32]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefTest, $desc,
    ]);

builder.addFunction('take_struct', makeSig([kWasmStructRef], [kWasmI32]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefTest, $desc,
    ]);

builder.addFunction('take_desc', makeSig([wasmRefType($desc)], [kWasmI32]))
    .exportFunc()
    .addBody([kExprI32Const, 1]);

let instance = builder.instantiate();
let wasm = instance.exports;
let desc = wasm.get_desc();

// Loop to hit both generic and compiled wrappers.
for (let i = 0; i < 3; i++) {
  assertEquals(1, wasm.take_any(desc));
  assertEquals(1, wasm.take_eq(desc));
  assertEquals(1, wasm.take_struct(desc));
  assertEquals(1, wasm.take_desc(desc));
}
