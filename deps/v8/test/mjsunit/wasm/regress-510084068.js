// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-wasmfx

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

let sig_ret = builder.addType(makeSig([], []));
let cont_ret = builder.addCont(sig_ret);
let sig_target = builder.addType(makeSig([wasmRefNullType(cont_ret)], []));
let cont_target = builder.addCont(sig_target);
let tag = builder.addTag(makeSig([], []));

builder.addFunction("dummy", sig_target).addBody([]).exportFunc();

let f_builder = builder.addFunction("f", makeSig([wasmRefNullType(cont_target)], []))
    .addBody([
        kExprTry, kWasmVoid,
            kExprLocalGet, 0,
            kExprSwitch, cont_target, tag,
        kExprCatchAll,
            kExprNop,
        kExprEnd,
    ]).exportFunc();

builder.addFunction("run", makeSig([], []))
    .addBody([
        kExprRefFunc, 0,
        kExprContNew, cont_target,
        kExprCallFunction, f_builder.index
    ]).exportFunc();

let instance = builder.instantiate();
assertThrows(() => instance.exports.run(),WebAssembly.RuntimeError);
