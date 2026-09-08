// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-wasmfx --future --no-liftoff --no-wasm-lazy-compilation
d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let sig_v_v = builder.addType(kSig_v_v);
let cont_v_v = builder.addCont(sig_v_v);
let tag = builder.addTag(kSig_v_v);

builder.addFunction("dummy", sig_v_v).addBody([]).exportFunc();

builder.addFunction("get_cont", makeSig([], [wasmRefType(cont_v_v)]))
    .addBody([
        kExprRefFunc, 0,
        kExprContNew, cont_v_v,
    ]).exportFunc();

let num_handlers = 40000;
let body = [
    kExprCallFunction, 1,
    kExprResume, ...wasmUnsignedLeb(cont_v_v), ...wasmUnsignedLeb(num_handlers),
];
for (let i = 0; i < num_handlers; i++) {
    body.push(0x01); // kSwitch
    body.push(...wasmUnsignedLeb(tag));
}
body.push(kExprUnreachable);

builder.addFunction("main", kSig_v_v)
    .addBody(body).exportFunc();

assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
             /invalid handler count \(> max WasmFX handlers\): 40000/);
