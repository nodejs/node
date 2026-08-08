// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx --expose-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

function makeModuleB() {
    let b = new WasmModuleBuilder();
    let params = new Array(100).fill(kWasmI32);
    let sig = b.addType(makeSig(params, []));
    let cont = b.addCont(sig);
    let global = b.addGlobal(wasmRefNullType(cont), true, false, [kExprRefNull, cont]);
    b.addFunction("f", sig).addBody([]).exportFunc();
    let store = b.addFunction("store", makeSig([wasmRefType(cont)], []))
        .addBody([kExprLocalGet, 0, kExprGlobalSet, global.index])
        .exportFunc();
    let resume_body = [];
    for (let i = 0; i < 100; i++) resume_body.push(kExprI32Const, i);
    resume_body.push(kExprGlobalGet, global.index, kExprRefAsNonNull);
    resume_body.push(kExprResume, cont, 0);
    b.addFunction("resume", makeSig([], []))
        .addBody(resume_body)
        .exportFunc();
    return b.instantiate();
}
function makeModuleA() {
    let b = new WasmModuleBuilder();
    let params = new Array(100).fill(kWasmI32);
    let sig = b.addType(makeSig(params, []));
    let cont = b.addCont(sig);
    let store_sig = b.addType(makeSig([wasmRefType(cont)], []));
    let import_idx = b.addImport("m", "store", store_sig);
    b.addFunction("go", makeSig([kWasmFuncRef], []))
        .addBody([
            kExprLocalGet, 0,
            kGCPrefix, kExprRefCast, sig,
            kExprContNew, cont,
            kExprCallFunction, import_idx
        ])
        .exportFunc();
    return b.instantiate({m: {store: instanceB.exports.store}});
}
let instanceB = makeModuleB();
let instanceA = makeModuleA();
instanceA.exports.go(instanceB.exports.f);
instanceA = null;
gc({execution: 'sync'});
instanceB.exports.resume();
