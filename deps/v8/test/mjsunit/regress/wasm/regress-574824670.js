// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors --wasm-merged-descriptors

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.startRecGroup();
let $desc = builder.nextTypeIndex() + 1;
let $s = builder.addStruct(
    {fields: [makeField(kWasmI32, true)], descriptor: $desc});
/* $desc */ builder.addStruct({fields: [], describes: $s});
let $sig_a_v = builder.addType(makeSig([], [kWasmAnyRef]));
builder.endRecGroup();

let $g_desc = builder.addGlobal(wasmRefType($desc).exact(), false, false,
                                [kGCPrefix, kExprStructNewDefault, $desc]);

builder.addFunction('make', $sig_a_v).exportFunc().addBody([
  kExprI32Const, 42,
  kExprGlobalGet, $g_desc.index,
  kGCPrefix, kExprStructNewDesc, $s,
]);

let instance = builder.instantiate();
let described_struct = instance.exports.make();

performance.measureMemory({execution: 'eager'});
