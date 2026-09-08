// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --trace-wasm-generate-compilation-hints
// Flags: --liftoff --wasm-lazy-compilation --no-disable-optimizing-compilers

// This test used to fail under ASAN.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();

let num_funcs = 1000;

builder.addFunction("f0", kSig_v_v)
  .addBody([kExprCallFunction, 1, kExprCallFunction, 1, kExprCallFunction, 1])
  .exportFunc();

builder.addFunction("f1", kSig_v_v)
  .addBody([])
  .exportFunc();

for (let i = 2; i < num_funcs; i++) {
  builder.addFunction("f" + i, kSig_i_i)
    .addBody([...wasmI32Const(i), kExprCallFunction, 0])
    .exportFunc();
}

let module = builder.toModule();

function worker_code() {
  onmessage = function ({data:msg}) {
    let instance = new WebAssembly.Instance(msg.module);
    let num_funcs = 1000;
    for (let i = 2; i < num_funcs; i++) {
      instance.exports['f' + i]();
    }
    postMessage("ok");
  }
}

let instance = new WebAssembly.Instance(module);

for (let i = 0; i < 50; i++) {
  instance.exports.f0();
}

let workers = [];

for (let i = 0; i < 4; i++) {
  let worker = new Worker(worker_code, {type: 'function'});
  worker.postMessage({module});
  workers.push(worker);
}

%GenerateWasmCompilationHints(instance);

for (let worker of workers) worker.terminate();
