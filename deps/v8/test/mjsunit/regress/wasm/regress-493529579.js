// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
let sig0 = builder.addType(makeSig([], []));
let cont = builder.addCont(sig0);
let import_idx = builder.addImport("m", "f", sig0);

builder.addFunction("cont_start", sig0)
    .addBody([
        kExprCallFunction, import_idx
    ]);
builder.addFunction("main", sig0)
    .addBody([
        kExprRefFunc, 1, // cont_start
        kExprContNew, cont,
        kExprResume, cont, 0, // 0 handlers
    ])
    .exportFunc();
builder.addDeclarativeElementSegment([1]);
try {
    let js_f = new WebAssembly.Suspending(() => { print("In JS"); });
    let instance = builder.instantiate({ m: { f: js_f } });
    instance.exports.main();
} catch (e) {
  assertTrue(e instanceof WebAssembly.SuspendError);
}
