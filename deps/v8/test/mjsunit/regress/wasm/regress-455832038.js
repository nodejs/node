// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx --expose-gc --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let sig_v_v = builder.addType(kSig_v_v);
let cont_index = builder.addCont(sig_v_v);
let jspi_suspending_index = builder.addImport("m", "jspi_suspending", sig_v_v);
builder.addExport("jspi_suspending", jspi_suspending_index);
builder.addFunction("cont_run_jspi_suspending", kSig_v_v)
    .addBody([
        kExprRefFunc, jspi_suspending_index,
        kExprContNew, cont_index,
        kExprResume, cont_index, 0,
    ]).exportFunc();
let instance;
instance = builder.instantiate( {m: {
  jspi_suspending: new WebAssembly.Suspending(() => {
    %SimulateNewspaceFull();
    gc();
  })
}});

WebAssembly.promising(instance.exports.cont_run_jspi_suspending)();

gc();
