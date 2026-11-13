// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared

// Tests that struct.atomic.set with kAtomicSeqCst on reference fields preserves
// sequential consistency (Dekker's store-buffering litmus test).

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestStructAtomicSetRefSeqCst() {
  const builder = new WasmModuleBuilder();
  const sharedI31RefNull = wasmRefNullType(kWasmI31Ref).shared();

  // Pad the struct to >64 bytes so two struct instances never share a cache
  // line, while keeping `step`, `seen`, and `val` on the same cache line.
  const fields = [
    makeField(kWasmI32, true),          // 0: step
    makeField(kWasmI32, true),          // 1: seen
    makeField(sharedI31RefNull, true),  // 2: val
    ...Array.from({length: 8}, () => makeField(kWasmI64, true)),
  ];
  const struct_type = builder.addStruct({fields, is_shared: true});
  const sharedStructRef = wasmRefNullType(struct_type);

  builder.addFunction('make', makeSig([], [sharedStructRef]))
      .addBody([kGCPrefix, kExprStructNewDefault, struct_type])
      .exportFunc();

  // Increment me.step and spin until other.step catches up.
  const barrier = [
    kExprLocalGet, 0,
    kExprLocalGet, 2,
    kExprI32Const, 1,
    kExprI32Add,
    kExprLocalTee, 2,
    kAtomicPrefix, kExprStructAtomicSet, kAtomicSeqCst, struct_type, 0,
    kExprLoop, kWasmVoid,
      kExprLocalGet, 1,
      kAtomicPrefix, kExprStructAtomicGet, kAtomicSeqCst, struct_type, 0,
      kExprLocalGet, 2,
      kExprI32LtS,
      kExprBrIf, 0,
    kExprEnd,
  ];

  // Symmetric Dekker store-buffering loop:
  // Thread 0 calls run(a, b); Thread 1 calls run(b, a).
  builder.addFunction('run', makeSig([sharedStructRef, sharedStructRef], []))
      .addLocals(kWasmI32, 1)          // local 2: step
      .addLocals(sharedI31RefNull, 1)  // local 3: tag
      .addBody([
        kExprLoop, kWasmVoid,
          ...barrier,

          // Store-Buffering critical section:
          // me.val = tag (seqcst)
          // me.seen = ref.eq(other.val (seqcst), tag)
          kExprLocalGet, 0,
          kExprLocalGet, 2,
          kGCPrefix, kExprRefI31Shared,
          kExprLocalTee, 3,
          kAtomicPrefix, kExprStructAtomicSet, kAtomicSeqCst, struct_type, 2,

          kExprLocalGet, 0,
          kExprLocalGet, 1,
          kAtomicPrefix, kExprStructAtomicGet, kAtomicSeqCst, struct_type, 2,
          kExprLocalGet, 3,
          kExprRefEq,
          kAtomicPrefix, kExprStructAtomicSet, kAtomicSeqCst, struct_type, 1,

          ...barrier,

          // Sequential consistency requires at least one thread to see the
          // other's store (me.seen | other.seen != 0).
          kExprLocalGet, 0,
          kAtomicPrefix, kExprStructAtomicGet, kAtomicSeqCst, struct_type, 1,
          kExprLocalGet, 1,
          kAtomicPrefix, kExprStructAtomicGet, kAtomicSeqCst, struct_type, 1,
          kExprI32Ior,
          kExprI32Eqz,
          kExprIf, kWasmVoid,
            kExprUnreachable,
          kExprEnd,

          kExprLocalGet, 2,
          ...wasmI32Const(100000),
          kExprI32LtS,
          kExprBrIf, 0,
        kExprEnd,
      ])
      .exportFunc();

  const module = builder.toModule();
  const instance = new WebAssembly.Instance(module);
  const a = instance.exports.make();
  const b = instance.exports.make();

  const worker = new Worker(function() {
    onmessage = function({data: {module, a, b}}) {
      new WebAssembly.Instance(module).exports.run(b, a);
      postMessage('done');
    };
  }, {type: 'function'});

  worker.postMessage({module, a, b});
  instance.exports.run(a, b);
  assertEquals('done', worker.getMessage());
  worker.terminate();
})();
