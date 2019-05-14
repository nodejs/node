// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
// Generate function 1 (out of 2).
sig0 = makeSig([], [kWasmI32]);
builder.addFunction(undefined, sig0)
  .addBody([
    kExprI64Const, 0xc8, 0xda, 0x9c, 0xbc, 0xf8, 0xf0, 0xe1, 0xc3, 0x87, 0x7f,
    kExprLoop, kWasmF64,
      ...wasmF64Const(0),
      ...wasmF64Const(0),
      ...wasmF64Const(0),
      ...wasmF64Const(0),
      ...wasmF64Const(0),
      ...wasmF64Const(0),
      ...wasmF64Const(0),
      ...wasmF64Const(0),
      ...wasmF64Const(0),
      ...wasmF64Const(0),
      ...wasmF64Const(0),
      ...wasmF64Const(0),
      kExprCallFunction, 0x01,
      ...wasmF64Const(0),
      kExprEnd,
    ...wasmF64Const(0),
    ...wasmF64Const(0),
    ...wasmF64Const(0),
    ...wasmF64Const(0),
    ...wasmF64Const(0),
    ...wasmF64Const(0),
    ...wasmF64Const(0),
    ...wasmF64Const(0),
    ...wasmF64Const(0),
    ...wasmF64Const(0),
    ...wasmF64Const(0),
    kExprCallFunction, 0x01,
    kExprI64Const, 0xb9, 0xf2, 0xe4, 0x01,
    kExprI64LtS]);
// Generate function 2 (out of 2).
sig1 = makeSig(new Array(12).fill(kWasmF64), []);
builder.addFunction(undefined, sig1).addBody([]);
builder.addExport('main', 0);
const instance = builder.instantiate();
assertEquals(1, instance.exports.main());

const builder2 = new WasmModuleBuilder();
sig0 = makeSig([], [kWasmI32]);
builder2.addFunction(undefined, sig0).addLocals({i64_count: 1}).addBody([
  kExprLoop, kWasmI32,     // loop i32
  kExprGetLocal, 0,        // get_local 3
  kExprF32SConvertI64,     // f32.sconvert/i64
  kExprI32ReinterpretF32,  // i32.reinterpret/f32
  kExprEnd                 // end
]);
builder2.addExport('main', 0);
const instance2 = builder2.instantiate();
assertEquals(0, instance2.exports.main());
