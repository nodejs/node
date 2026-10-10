// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-wasmfx --trace-wasm --no-wasm-lazy-compilation

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
let builder = new WasmModuleBuilder();
let sig_v_v = builder.addType(kSig_v_v);
let cont_v_v = builder.addCont(sig_v_v);
let sig_v_c = builder.addType(makeSig([], [wasmRefNullType(cont_v_v)]));
let tag = builder.addTag(kSig_v_v);

let dummy = builder.addFunction("dummy", sig_v_v).addBody([
]).exportFunc();
let get_cont = builder.addFunction("get_cont", makeSig([], [wasmRefType(cont_v_v)]))
    .addBody([
        kExprRefFunc, dummy.index,
        kExprContNew, cont_v_v,
    ]).exportFunc();

let body = [
    kExprCallFunction, get_cont.index,
    kExprResume, ...wasmUnsignedLeb(cont_v_v), ...wasmUnsignedLeb(1),
    0x00, ...wasmUnsignedLeb(tag), 0x00,
    kExprRefNull, cont_v_v,
];
builder.addFunction("main", sig_v_c)
    .addBody(body).exportFunc();
let instance = builder.instantiate();
