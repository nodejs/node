// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --wasm-wasmfx --expose-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/sandbox/wasm-jspi.js');

let builder = new WasmModuleBuilder();
let sig_v_v = builder.addType(kSig_v_v);
let cont_index = builder.addCont(sig_v_v);

let resolveB;
let promiseB = new Promise(r => { resolveB = r; });
let wasm_promiseB;
let suspenderB;

let suspendB_index = builder.addImport('m', 'suspendB', kSig_v_v);
let suspendA_index = builder.addImport('m', 'suspendA', kSig_v_v);
let call_promising_nop_index =
    builder.addImport('m', 'call_promising_nop', kSig_v_v);
let trigger_index = builder.addImport('m', 'trigger', kSig_v_v);

builder.addFunction('nop', kSig_v_v).addBody([]).exportFunc();

let cont_main = builder.addFunction('cont_main', kSig_v_v)
    .addBody([
      kExprCallFunction, call_promising_nop_index,
    ]).exportFunc();

builder.addFunction('runB', kSig_v_v)
    .addBody([
      // 1. Suspend once so JS can read suspenderB's handle.
      kExprCallFunction, suspendB_index,
      // 2. Start a WasmFX continuation X that calls a promising export (nop),
      // setting suspenderB->stack_ = X, and then returns, retiring X to
      // the StackPool.
      kExprRefFunc, cont_main.index,
      kExprContNew, cont_index,
      kExprResume, cont_index, 0,
      // 3. While suspenderB is still Active, free X from the StackPool,
      // corrupt wasm_promiseB's reaction to point to suspenderB, and capture
      // an async stack trace.
      kExprCallFunction, trigger_index,
    ]).exportFunc();

builder.addExport('suspendA', suspendA_index);

let instance = builder.instantiate({
  m: {
    suspendB: new WebAssembly.Suspending(() => promiseB),
    suspendA: new WebAssembly.Suspending(() => wasm_promiseB),
    call_promising_nop: () => {
      WebAssembly.promising(instance.exports.nop)();
    },
    trigger: () => {
      gc({type: 'major', execution: 'sync', flavor: 'last-resort'});
      set_suspender(get_resume_data(wasm_promiseB), suspenderB);
      new Error().stack;
      assertUnreachable('Process should have been killed.');
    },
  }
});

wasm_promiseB = WebAssembly.promising(instance.exports.runB)();
WebAssembly.promising(instance.exports.suspendA)();
suspenderB = get_suspender(get_resume_data(promiseB));
resolveB();
