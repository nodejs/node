// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const struct = builder.addStruct({
  fields: [makeField(kWasmI32, true)],
  shared: true,
});
const array8 = builder.addArray(kWasmI8, {shared: true});
const array16 = builder.addArray(kWasmI16, {shared: true});

builder.addFunction('makeStruct', makeSig([kWasmI32], [wasmRefType(struct)]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructNew, struct])
  .exportFunc();

builder.addFunction('makeArray8', makeSig([], [wasmRefType(array8)]))
  .addBody([
    kExprI32Const, 0,
    kExprI32Const, 0,
    kGCPrefix, kExprArrayNewFixed, array8, 2,
  ])
  .exportFunc();

builder.addFunction('makeArray16', makeSig([], [wasmRefType(array16)]))
  .addBody([
    kExprI32Const, 0,
    kExprI32Const, 0,
    kGCPrefix, kExprArrayNewFixed, array16, 2,
  ])
  .exportFunc();

function addWaitAndRead(name, array_type, atomic_get_op) {
  builder.addFunction(
      name,
      makeSig([wasmRefType(struct), wasmRefType(array_type)],
              [kWasmI32, kWasmI32, kWasmI32]))
    .addLocals(kWasmI32, 1)
    .addBody([
      // 1. Non-atomic read of struct field 0 (initially 0).
      kExprLocalGet, 0,
      kGCPrefix, kExprStructGet, struct, 0,
      kExprLocalTee, 2,
      // 2. Signal the worker via a non-atomic array.set on index 1 (dependent
      // on the struct load so it cannot be reordered before the load, and
      // non-atomic so WasmLoadEliminationReducer does not invalidate shared
      // struct state here).
      kExprLocalGet, 1,
      kExprI32Const, 1,
      kExprLocalGet, 2,
      kExprI32Const, 1,
      kExprI32Add,
      kGCPrefix, kExprArraySet, array_type,
      // 3. Spin until index 0 becomes non-zero using array.atomic.get_[s|u].
      kExprLoop, kWasmI32,
        kExprLocalGet, 1,
        kExprI32Const, 0,
        kAtomicPrefix, atomic_get_op, kAtomicSeqCst, array_type,
        kExprLocalTee, 2,
        kExprI32Eqz,
        kExprBrIf, 0,
        kExprLocalGet, 2,
      kExprEnd,
      // 4. Non-atomic read of struct field 0; must not be load-eliminated
      // across the synchronizing array.atomic.get_[s|u] in the loop.
      kExprLocalGet, 0,
      kGCPrefix, kExprStructGet, struct, 0,
    ])
    .exportFunc();
}

addWaitAndRead('waitAndReadS8', array8, kExprArrayAtomicGetS);
addWaitAndRead('waitAndReadU8', array8, kExprArrayAtomicGetU);
addWaitAndRead('waitAndReadS16', array16, kExprArrayAtomicGetS);
addWaitAndRead('waitAndReadU16', array16, kExprArrayAtomicGetU);

function addSignal(name, array_type, atomic_get_op) {
  builder.addFunction(
      name,
      makeSig([wasmRefType(struct), wasmRefType(array_type)], []))
    .addBody([
      kExprLoop, kWasmVoid,
        kExprLocalGet, 1,
        kExprI32Const, 1,
        kAtomicPrefix, atomic_get_op, kAtomicSeqCst, array_type,
        kExprI32Eqz,
        kExprBrIf, 0,
      kExprEnd,
      kExprLocalGet, 0,
      kExprI32Const, 42,
      kAtomicPrefix, kExprStructAtomicSet, kAtomicSeqCst, struct, 0,
      kExprLocalGet, 1,
      kExprI32Const, 0,
      ...wasmI32Const(-1),
      kAtomicPrefix, kExprArrayAtomicSet, kAtomicSeqCst, array_type,
    ])
    .exportFunc();
}

addSignal('signal8', array8, kExprArrayAtomicGetU);
addSignal('signal16', array16, kExprArrayAtomicGetU);

const module = builder.toModule();
const instance = new WebAssembly.Instance(module);
const wasm = instance.exports;

const worker = new Worker(function() {
  let inst;
  onmessage = function({data: msg}) {
    if (msg.module) {
      inst = new WebAssembly.Instance(msg.module);
      postMessage('ready');
      return;
    }
    inst.exports[msg.signal](msg.s, msg.a);
  };
}, {type: 'function'});

worker.postMessage({module});
assertEquals('ready', worker.getMessage());

for (const [waitFn, makeArrayFn, signalFn, expectedArrayVal] of [
  ['waitAndReadS8', 'makeArray8', 'signal8', -1],
  ['waitAndReadU8', 'makeArray8', 'signal8', 0xff],
  ['waitAndReadS16', 'makeArray16', 'signal16', -1],
  ['waitAndReadU16', 'makeArray16', 'signal16', 0xffff],
]) {
  const s = wasm.makeStruct(0);
  const a = wasm[makeArrayFn]();
  worker.postMessage({signal: signalFn, s, a});
  assertEquals([0, expectedArrayVal, 42], wasm[waitFn](s, a));
}

worker.terminate();
