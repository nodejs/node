// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $mem0 = builder.addMemory64(2, undefined);

builder.addFunction("corrupt_load", kSig_l_l)
  .addBody([
    kExprI64Const, 0,
    ...wasmI32Const(65),
    kExprLocalGet, 0,
    kNumericPrefix, kExprMemoryFill, $mem0,
    kExprI64Const, 0,
    kExprI64LoadMem, 3, 0,
  ])
  .exportFunc();

const instance = builder.instantiate();
const ex = instance.exports;

ex.corrupt_load(8n);
const r = ex.corrupt_load(256n);

assertEquals(r, 0x4141414141414141n);
