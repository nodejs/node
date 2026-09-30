// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-wrapper-tiering-budget=250

// WasmWrapperCache::ModificationScope::AddWrapper updated the shared
// WasmCodePointerTable entry before flushing the instruction cache. On ARM64
// CPUs with non-coherent instruction/data caches (e.g. Apple Silicon), a worker
// thread concurrently calling an import of the same signature could jump to the
// newly published wrapper address and fetch stale L1I cache lines (SIGILL).

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestConcurrentWasmToJSWrapperTierUp() {
  const kNumSigs = 64;
  const kImportsPerSig = 16;
  const kNumReaders = 3;
  // Lowering `--wasm-wrapper-tiering-budget` from its default (1000) to 250
  // makes the compiler thread trigger tier-up 4x faster, while 16 imports *
  // 249 calls = 3984 reader calls (~60us on Apple Silicon) spans the
  // CompileWrapper window without any single reader import's budget reaching 0.
  const kBudget = 250;

  // 1. Build a Wasm module with 64 distinct 7-parameter signatures (`s = 0..63`)
  // and 16 imports (`f_<s>_0..15`) per signature:
  // - `compile_<s>(count)`: calls `f_<s>_0` `count` times to trigger tier-up.
  // - `read_<s>(max_iters)`: calls `f_<s>_0..15` while `--max_iters > 0` and
  //   `Atomics.load(mem, 4) <= s + 1` (so readers keep calling through `s`
  //   after `AddWrapper(s)` finishes on native hardware, but bail out early on
  //   slow simulator builds once the compiler thread moves past `s + 1`).
  const builder = new WasmModuleBuilder();
  builder.addImportedMemory('m', 'mem', 1, 1, true);
  const makeParams = (s) => [0, 1, 2, 3, 4, 5, 6].map(
      b => (s & (1 << b)) ? kWasmExternRef : kWasmI32);

  for (let s = 0; s < kNumSigs; s++) {
    const sig = builder.addType(makeSig(makeParams(s), [kWasmI32]));
    for (let j = 0; j < kImportsPerSig; j++) {
      builder.addImport('m', `f_${s}_${j}`, sig);
    }
  }

  for (let s = 0; s < kNumSigs; s++) {
    const args = makeParams(s).flatMap(
        t => t === kWasmExternRef ? [kExprRefNull, kExternRefCode]
                                  : [kExprI32Const, 0]);
    const callOne = (j) => [
      ...args,
      kExprCallFunction, ...wasmUnsignedLeb(s * kImportsPerSig + j),
      kExprDrop,
    ];
    const callAll = [];
    for (let j = 0; j < kImportsPerSig; j++) callAll.push(...callOne(j));

    builder.addFunction(`compile_${s}`, kSig_v_i)
        .addBody([
          kExprLoop, kWasmVoid,
            ...callOne(0),
            kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub, kExprLocalTee, 0,
            kExprBrIf, 0,
          kExprEnd,
        ])
        .exportFunc();

    builder.addFunction(`read_${s}`, kSig_v_i)
        .addBody([
          kExprLoop, kWasmVoid,
            ...callAll,
            kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub, kExprLocalTee, 0,
            kExprI32Const, 0, kExprI32GtS,
            kExprI32Const, 4, kAtomicPrefix, kExprI32AtomicLoad, 2, 0,
            kExprI32Const, ...wasmSignedLeb(s + 1), kExprI32LeS,
            kExprI32And,
            kExprBrIf, 0,
          kExprEnd,
        ])
        .exportFunc();
  }

  // 2. Shared synchronization state inside the imported WebAssembly.Memory:
  // - `sync[0]` (byte offset 0): count of workers that finished instantiation.
  // - `sync[1]` (byte offset 4): current signature `s` being compiled.
  const module = builder.toModule();
  const memory = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
  const sync = new Int32Array(memory.buffer);
  Atomics.store(sync, 1, -1);

  // 3. Reader workers: instantiate the module, signal readiness (`sync[0]`),
  // and spin-wait for each `s` before calling `read_<s>(kBudget - 1)`.
  function workerCode() {
    onmessage = function({data: {module, memory, kNumSigs, kImportsPerSig, kBudget}}) {
      const sync = new Int32Array(memory.buffer);
      const m = {mem: memory};
      for (let s = 0; s < kNumSigs; s++) {
        for (let j = 0; j < kImportsPerSig; j++) m[`f_${s}_${j}`] = () => 1;
      }
      const {exports} = new WebAssembly.Instance(module, {m});

      Atomics.add(sync, 0, 1);
      for (let s = 0; s < kNumSigs; s++) {
        while (Atomics.load(sync, 1) < s) {}
        exports[`read_${s}`](kBudget - 1);
      }
      postMessage('ok');
    };
  }

  const workers = [];
  for (let w = 0; w < kNumReaders; w++) {
    const worker = new Worker(workerCode, {type: 'function'});
    worker.postMessage({module, memory, kNumSigs, kImportsPerSig, kBudget});
    workers.push(worker);
  }

  // 4. Main thread (compiler): wait for all workers to instantiate, then step
  // through `s = 0..47`, releasing readers on `s` and calling `compile_<s>`.
  const m = {mem: memory};
  for (let s = 0; s < kNumSigs; s++) {
    for (let j = 0; j < kImportsPerSig; j++) m[`f_${s}_${j}`] = () => 1;
  }
  const {exports} = new WebAssembly.Instance(module, {m});

  while (Atomics.load(sync, 0) < workers.length) {}

  for (let s = 0; s < kNumSigs; s++) {
    Atomics.store(sync, 1, s);
    exports[`compile_${s}`](kBudget);
  }
  Atomics.store(sync, 1, kNumSigs + 1);

  for (const w of workers) {
    assertEquals('ok', w.getMessage());
    w.terminate();
  }
})();
