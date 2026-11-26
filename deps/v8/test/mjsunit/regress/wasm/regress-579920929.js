// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestLoadEliminationAcrossAtomicFence() {
  const builder = new WasmModuleBuilder();
  const struct_type = builder.addStruct({
    fields: [
      makeField(kWasmI32, true),  // 0: data
      makeField(kWasmI32, true),  // 1: start
      makeField(kWasmI32, true),  // 2: worker_ready
    ],
    is_shared: true,
  });
  const sharedStructRef = wasmRefNullType(struct_type);

  builder.addFunction('make', makeSig([], [sharedStructRef]))
      .addBody([kGCPrefix, kExprStructNewDefault, struct_type])
      .exportFunc();

  builder.addFunction('worker', makeSig([sharedStructRef], []))
      .addBody([
        kExprLocalGet, 0,
        kExprI32Const, 1,
        kAtomicPrefix, kExprStructAtomicSet, kAtomicSeqCst, struct_type, 2,
        kExprLoop, kWasmVoid,
          kExprLocalGet, 0,
          kAtomicPrefix, kExprStructAtomicGet, kAtomicSeqCst, struct_type, 1,
          kExprI32Eqz,
          kExprBrIf, 0,
        kExprEnd,
        kExprLocalGet, 0,
        kExprI32Const, 42,
        kAtomicPrefix, kExprStructAtomicSet, kAtomicSeqCst, struct_type, 0,
      ])
      .exportFunc();

  builder.addFunction('wait_worker_ready', makeSig([sharedStructRef], []))
      .addBody([
        kExprLoop, kWasmVoid,
          kExprLocalGet, 0,
          kAtomicPrefix, kExprStructAtomicGet, kAtomicSeqCst, struct_type, 2,
          kExprI32Eqz,
          kExprBrIf, 0,
        kExprEnd,
      ])
      .exportFunc();

  builder.addFunction('main', makeSig([sharedStructRef], [kWasmI32]))
      .addLocals(kWasmI32, 2)
      .addBody([
        // 1. Initial load of s.data (0) before signaling worker.
        kExprLocalGet, 0,
        kGCPrefix, kExprStructGet, struct_type, 0,
        kExprLocalSet, 2,

        // 2. Signal worker (s.start = 1) + atomic.fence.
        kExprLocalGet, 0,
        kExprI32Const, 1,
        kGCPrefix, kExprStructSet, struct_type, 1,
        kAtomicPrefix, kExprAtomicFence, kAtomicSeqCst,

        // 3. Poll s.data across atomic.fence.
        //    WasmLoadEliminationReducer ignores kMemoryBarrier and replaces
        //    struct.get(s.data) inside the loop with the pre-fence load (0).
        kExprLoop, kWasmVoid,
          kAtomicPrefix, kExprAtomicFence, kAtomicSeqCst,
          kExprLocalGet, 0,
          kGCPrefix, kExprStructGet, struct_type, 0,
          kExprLocalTee, 2,
          kExprIf, kWasmVoid,
            kExprLocalGet, 2,
            kExprReturn,
          kExprEnd,
          kExprLocalGet, 1,
          kExprI32Const, 1,
          kExprI32Add,
          kExprLocalTee, 1,
          ...wasmI32Const(100000000),
          kExprI32LtS,
          kExprBrIf, 0,
        kExprEnd,
        kExprLocalGet, 2,
      ])
      .exportFunc();

  const module = builder.toModule();
  const instance = new WebAssembly.Instance(module);
  const s = instance.exports.make();

  const worker = new Worker(function() {
    onmessage = function({data: {module, s}}) {
      new WebAssembly.Instance(module).exports.worker(s);
      postMessage('done');
    };
  }, {type: 'function'});

  worker.postMessage({module, s});
  instance.exports.wait_worker_ready(s);
  const result = instance.exports.main(s);
  assertEquals('done', worker.getMessage());
  worker.terminate();
  assertEquals(42, result);
})();
