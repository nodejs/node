// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.startRecGroup();
let $top = builder.addStruct([]);
let $mid = builder.addStruct([], $top);
let $bot_desc = builder.nextTypeIndex() + 1;
let $bot = builder.addStruct({fields: [], supertype: $mid, descriptor: $bot_desc});
let verify = builder.addStruct({fields: [], describes: $bot});
builder.endRecGroup();
assertEquals(verify, $bot_desc);

let $g = builder.addGlobal(wasmRefType($bot), false, false, [
    kGCPrefix, kExprStructNewDefault, $bot_desc,
    kGCPrefix, kExprStructNewDefaultDesc, $bot
]);

builder.addFunction("main", kSig_v_v).exportFunc().addBody([
    kExprGlobalGet, $g.index,
    kGCPrefix, kExprRefCast, kWasmExact, $bot,
    kExprDrop,
]);

let instance = builder.instantiate();
instance.exports.main();
