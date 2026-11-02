// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors --wasm-merged-descriptors

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.startRecGroup();
let $desc = builder.nextTypeIndex() + 1;
let $s = builder.addStruct(
    {fields: [makeField(kWasmI32, true)], descriptor: $desc});
/* $desc */ builder.addStruct({fields: [], describes: $s});
let $regular = builder.addStruct({});
let $sig_a_v = builder.addType(makeSig([], [kWasmAnyRef]));
builder.endRecGroup();

let $g_desc = builder.addGlobal(wasmRefType($desc).exact(), false, false,
                                [kGCPrefix, kExprStructNewDefault, $desc]);

builder.addFunction("make", $sig_a_v).exportFunc().addBody([
  kExprI32Const, 42,
  kExprGlobalGet, $g_desc.index,
  kGCPrefix, kExprStructNewDesc, $s,
]);
builder.addFunction("make_regular", $sig_a_v).exportFunc().addBody([
  kGCPrefix, kExprStructNew, $regular,
]);
builder.addFunction("get_desc", $sig_a_v).exportFunc().addBody([
  kExprGlobalGet, $g_desc.index,
]);

let instance = builder.instantiate();
// For regular Wasm structs, we use context-independent maps ("canonical RTTs"),
// so we cannot easily track their creation context.
let regular_struct = instance.exports.make_regular();
assertThrows(() => Realm.owner(regular_struct), Error,
             "object doesn't have creation context");
// Custom Descriptors are NC-specific so that they can hold a prototype,
// so objects created from them can also know their own creation context.
let described_struct = instance.exports.make();
assertEquals(0, Realm.owner(described_struct));
let descriptor = instance.exports.get_desc();
assertEquals(0, Realm.owner(descriptor));
