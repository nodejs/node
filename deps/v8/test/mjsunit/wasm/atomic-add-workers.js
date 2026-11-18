// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --liftoff --no-wasm-tier-up

// Atomic adds from several threads to one shared location lose no update.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestAtomicAddFromWorkers() {
  const kIterations = 20000;
  const kWorkers = 3;
  const kThreads = kWorkers + 1;

  const builder = new WasmModuleBuilder();
  builder.addImportedMemory('m', 'memory', 1, 1, 'shared');
  // Returns once all {threads} threads have called it, so that they all
  // start adding at the same time.
  builder.addFunction('waitForAll', kSig_v_i)
      .addBody([
        kExprI32Const, 16,
        kExprI32Const, 1,
        kAtomicPrefix, kExprI32AtomicAdd, 2, 0,
        kExprDrop,
        kExprLoop, kWasmVoid,
          kExprI32Const, 16,
          kAtomicPrefix, kExprI32AtomicLoad, 2, 0,
          kExprLocalGet, 0,
          kExprI32Ne,
          kExprBrIf, 0,
        kExprEnd,
      ])
      .exportFunc();
  function addLoop(load_add) {
    return [
      kExprLoop, kWasmVoid,
        ...load_add,
        kExprLocalGet, 0,
        kExprI32Const, 1,
        kExprI32Sub,
        kExprLocalTee, 0,
        kExprBrIf, 0,
      kExprEnd,
    ];
  }
  builder.addFunction('add32', kSig_v_i)
      .addBody(addLoop([
        kExprI32Const, 0,
        kExprI32Const, 1,
        kAtomicPrefix, kExprI32AtomicAdd, 2, 0,
        kExprDrop,
      ]))
      .exportFunc();
  builder.addFunction('add64', kSig_v_i)
      .addBody(addLoop([
        kExprI32Const, 8,
        kExprI64Const, 1,
        kAtomicPrefix, kExprI64AtomicAdd, 3, 0,
        kExprDrop,
      ]))
      .exportFunc();
  const module = builder.toModule();
  const memory =
      new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});

  const workers = [];
  for (let i = 0; i < kWorkers; i++) {
    const worker = new Worker(function() {
      onmessage = function({data: {module, memory, threads, iterations}}) {
        const exports =
            new WebAssembly.Instance(module, {m: {memory}}).exports;
        exports.waitForAll(threads);
        exports.add32(iterations);
        exports.add64(iterations);
        postMessage('done');
      };
    }, {type: 'function'});
    worker.postMessage(
        {module, memory, threads: kThreads, iterations: kIterations});
    workers.push(worker);
  }
  const exports = new WebAssembly.Instance(module, {m: {memory}}).exports;
  exports.waitForAll(kThreads);
  exports.add32(kIterations);
  exports.add64(kIterations);
  for (const worker of workers) {
    assertEquals('done', worker.getMessage());
    worker.terminate();
  }

  const total = kThreads * kIterations;
  assertEquals(total, new Int32Array(memory.buffer)[0]);
  assertEquals(BigInt(total), new BigInt64Array(memory.buffer)[1]);
})();
