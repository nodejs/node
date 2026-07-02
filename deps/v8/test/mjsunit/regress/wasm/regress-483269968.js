// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --experimental-wasm-wasmfx --expose-gc --single-threaded

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let sig_r_r = builder.addType(kSig_r_r);
let sig_r_v = builder.addType(kSig_r_v);
let cont_r_r = builder.addCont(sig_r_r);
let cont_r_v = builder.addCont(sig_r_v);

let g = builder.addGlobal(kWasmExternRef, true).exportAs('g');
let fill = builder.addImport("m", "fill", kSig_v_v);

let target = builder.addFunction("target", sig_r_r)
    .addBody([
        kExprLocalGet, 0,
        kExprGlobalSet, g.index,
        kExprLocalGet, 0
    ]).exportFunc();

builder.addFunction("trigger", kSig_v_r)
    .addBody([
        kExprLocalGet, 0,
        kExprRefFunc, target.index,
        kExprContNew, cont_r_r,
        kExprCallFunction, fill,
        kExprContBind, cont_r_r, cont_r_v,
        kExprResume, cont_r_v, 0,
        kExprDrop,
    ]).exportFunc();

let instance = builder.instantiate({m: {fill: () => %SimulateNewspaceFull()}});

let ref = {};
instance.exports.trigger(ref);

let stale = instance.exports.g.value;
assertEquals(ref, stale);
