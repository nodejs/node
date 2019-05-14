// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-wasm-disable-structured-cloning

load("test/mjsunit/wasm/wasm-module-builder.js");

(function TestPostInterpretedModule() {
  let builder = new WasmModuleBuilder();
  let add = builder.addFunction("add", kSig_i_ii)
    .addBody([kExprGetLocal, 0, kExprGetLocal, 1, kExprI32Add])
    .exportFunc();

  let module = builder.toModule();
  let instance = new WebAssembly.Instance(module);
  let exp = instance.exports;

  let workerScript = `
    var instance = null;
    onmessage = function(message) {
      try {
        if (message.command == 'module') {
          instance = new WebAssembly.Instance(message.module);
          postMessage('OK');
        }
        if (message.command == 'call') {
          let result = instance.exports.add(40, 2);
          postMessage(result);
        }
      } catch(e) {
        postMessage('ERROR: ' + e);
      }
    }
  `;
  let worker = new Worker(workerScript, {type: 'string'});

  // Call method without using the interpreter.
  var initial_interpreted = %WasmNumInterpretedCalls(instance);
  assertEquals(23, exp.add(20, 3));
  assertEquals(initial_interpreted + 0, %WasmNumInterpretedCalls(instance));

  // Send module to the worker, still not interpreting.
  worker.postMessage({ command:'module', module:module });
  assertEquals('OK', worker.getMessage());
  worker.postMessage({ command:'call' });
  assertEquals(42, worker.getMessage());
  assertEquals(initial_interpreted + 0, %WasmNumInterpretedCalls(instance));

  // Switch to the interpreter and call method.
  %RedirectToWasmInterpreter(instance, add.index);
  assertEquals(23, exp.add(20, 3));
  assertEquals(initial_interpreted + 1, %WasmNumInterpretedCalls(instance));

  // Let worker call interpreted function.
  worker.postMessage({ command:'call' });
  assertEquals(42, worker.getMessage());
  assertEquals(initial_interpreted + 1, %WasmNumInterpretedCalls(instance));

  // All done.
  worker.terminate();
})();
