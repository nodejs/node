// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.startRecGroup();

let $top_desc = builder.nextTypeIndex() + 1;
let $top = builder.addStruct({fields: [], descriptor: $top_desc});
let top_verify = builder.addStruct({fields: [], describes: $top});
assertEquals(top_verify, $top_desc);

let $mid_desc = builder.nextTypeIndex() + 1;
let $mid = builder.addStruct({fields: [], supertype: $top, descriptor: $mid_desc});
let mid_verify = builder.addStruct({fields: [], supertype: $top_desc, describes: $mid});
assertEquals(mid_verify, $mid_desc);

let $bot_desc = builder.nextTypeIndex() + 1;
let $bot = builder.addStruct({fields: [], supertype: $mid, descriptor: $bot_desc});
let bot_verify = builder.addStruct({fields: [], supertype: $mid_desc, describes: $bot});
assertEquals(bot_verify, $bot_desc);

builder.endRecGroup();

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
