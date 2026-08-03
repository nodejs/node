// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --verify-heap --shared-heap

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  let num_pages = 64 * 1024 * 1024 / kPageSize;
  let builder = new WasmModuleBuilder();
  builder.addMemory64(num_pages, num_pages);
  builder.exportMemoryAs('memory');

  let module = builder.instantiate();
  let memory = module.exports.memory;
  new Int8Array(memory.buffer);
})();

function InstantiatingWorkerCode() {
  function workerAssert(condition, message) {
    if (!condition) postMessage(`Check failed: ${message}`);
  }

  onmessage = function({data:[mem, module]}) {
    workerAssert(mem instanceof WebAssembly.Memory, 'Wasm memory');
    workerAssert(mem.buffer instanceof SharedArrayBuffer, 'SAB');
    try {
      new WebAssembly.Instance(module, {imp: {mem: mem}});
      postMessage('Instantiation succeeded');
    } catch (e) {
      postMessage(`Exception: ${e}`);
    }
  };
}

(function TestImportMemory64AsMemory32InWorker() {
  print(arguments.callee.name);
  const builder1 = new WasmModuleBuilder();
  builder1.addMemory64(1, 1, /* shared */ true);
  builder1.exportMemoryAs('mem64');
  const instance1 = builder1.instantiate();
  const {mem64} = instance1.exports;

  let builder2 = new WasmModuleBuilder();
  builder2.addImportedMemory('imp', 'mem');
  let module2 = builder2.toModule();

  let worker = new Worker(InstantiatingWorkerCode, {type: 'function'});
  worker.postMessage([mem64, module2]);
})();

(function TestImportMemory32AsMemory64InWorker() {
  print(arguments.callee.name);
  const builder1 = new WasmModuleBuilder();
  builder1.addMemory(1, 1, /* shared */ true);
  builder1.exportMemoryAs('mem32');
  const instance1 = builder1.instantiate();
  const {mem32} = instance1.exports;

  let builder2 = new WasmModuleBuilder();
  builder2.addImportedMemory(
      'imp', 'mem', 1, 1, /* shared */ false, /* memory64 */ true);
  let module2 = builder2.toModule();

  let worker = new Worker(InstantiatingWorkerCode, {type: 'function'});
  worker.postMessage([mem32, module2]);
})();
