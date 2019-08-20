// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared-engine --no-wasm-disable-structured-cloning

load("test/mjsunit/wasm/wasm-module-builder.js");

(function TestPostModule() {
  let builder = new WasmModuleBuilder();
  builder.addFunction("add", kSig_i_ii)
    .addBody([kExprGetLocal, 0, kExprGetLocal, 1, kExprI32Add])
    .exportFunc();

  let module = builder.toModule();

  let workerScript = `
    onmessage = function(module) {
      try {
        let instance = new WebAssembly.Instance(module);
        let result = instance.exports.add(40, 2);
        postMessage(result);
      } catch(e) {
        postMessage('ERROR: ' + e);
      }
    }
  `;

  let worker = new Worker(workerScript, {type: 'string'});
  worker.postMessage(module);
  assertEquals(42, worker.getMessage());
  worker.terminate();
})();
