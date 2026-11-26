// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-memory-corruption-api --jitless --wasm-jitless

// DrumBrake regression test: atomic.wait and atomic.notify operate on memory
// through a trusted pointer rather than the WasmMemoryObject array_buffer.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const kHeapObjectTag = 1;
let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
function getField(obj, offset) {
  return memory.getUint32(obj + offset - kHeapObjectTag, true);
}
function setField(obj, offset, value) {
  memory.setUint32(obj + offset - kHeapObjectTag, value, true);
}

// Use an imported (rather than module-owned) shared memory so the same
// backing WebAssembly.Memory can be shared with a worker below, while each
// side still gets its own (independently corruptible) WasmMemoryObject.
function makeModule() {
  const builder = new WasmModuleBuilder();
  builder.addImportedMemory('m', 'memory', 1, 1, true /*shared*/);
  builder.addFunction(
             'wait32', makeSig([kWasmI32, kWasmI32, kWasmI64], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2,
        kAtomicPrefix, kExprI32AtomicWait, /*align*/ 0x02, /*offset*/ 0x00,
      ])
      .exportFunc();
  builder.addFunction(
             'wait64', makeSig([kWasmI32, kWasmI64, kWasmI64], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2,
        kAtomicPrefix, kExprI64AtomicWait, /*align*/ 0x03, /*offset*/ 0x00,
      ])
      .exportFunc();
  builder.addFunction('notify32', makeSig([kWasmI32, kWasmI32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0, kExprLocalGet, 1,
        kAtomicPrefix, kExprAtomicNotify, /*align*/ 0x02, /*offset*/ 0x00,
      ])
      .exportFunc();
  return builder.toModule();
}

const module = makeModule();
const sharedMemory =
    new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
const instance = new WebAssembly.Instance(module, {m: {memory: sharedMemory}});
const {wait32, wait64, notify32} = instance.exports;
const memObj = sharedMemory;

// Spin up a worker and hand it the (not yet corrupted) shared memory, so it
// gets its own, independent, uncorrupted WasmMemoryObject referring to the
// same backing store. This must happen before the array_buffer field is
// corrupted below, since postMessage needs to serialize the still-valid
// WasmMemoryObject.
function workerCode() {
  let instance;
  onmessage = function({data: msg}) {
    if (!instance) {
      instance = new WebAssembly.Instance(msg.module, msg.imports);
      postMessage('instantiated');
      return;
    }
    if (msg.action === 'wait32') {
      postMessage(instance.exports.wait32(...msg.arguments));
      return;
    }
    postMessage(`Invalid action: ${msg.action}`);
  }
}
const worker = new Worker(workerCode, {type: 'function'});
worker.postMessage({module, imports: {m: {memory: sharedMemory}}});
assertEquals('instantiated', worker.getMessage());

// Write distinct, known values into the real backing memory.
const real32 = new Int32Array(sharedMemory.buffer);
real32[0] = 0x11223344;
const real64 = new BigInt64Array(sharedMemory.buffer);
real64[1] = 0x1122334455667788n;  // offset 8, clear of the i32 slot above.

// Control: with an uncorrupted WasmMemoryObject, wait on the real value
// (with an immediate timeout) times out, and on a different value returns
// not-equal right away.
assertEquals(kAtomicWaitTimedOut, wait32(0, 0x11223344, 0n));
assertEquals(kAtomicWaitNotEqual, wait32(0, 0xdeadbeef | 0, 0n));
assertEquals(kAtomicWaitTimedOut, wait64(8, 0x1122334455667788n, 0n));

// Corrupt the WasmMemoryObject's array_buffer field to point at a fresh
// SharedArrayBuffer holding different values at the same offsets.
const badBuffer = new SharedArrayBuffer(65536);
new Int32Array(badBuffer)[0] = 0xdeadbeef | 0;
new BigInt64Array(badBuffer)[1] = 0x8877665544332211n;

const tid = Sandbox.getInstanceTypeIdOf(memObj);
const arrayBufferOffset = Sandbox.getFieldOffset(tid, 'array_buffer');
setField(Sandbox.getAddressOf(memObj) + kHeapObjectTag, arrayBufferOffset,
         Sandbox.getAddressOf(badBuffer) + kHeapObjectTag);

// If atomic.wait were reading through the corrupted array_buffer, it would
// now observe the substituted (different) values. Expect it to still see
// the real memory contents.
assertEquals(kAtomicWaitTimedOut, wait32(0, 0x11223344, 0n));
assertEquals(kAtomicWaitNotEqual, wait32(0, 0xdeadbeef | 0, 0n));
assertEquals(kAtomicWaitTimedOut, wait64(8, 0x1122334455667788n, 0n));

// If atomic.notify were computing the wakeup address from the corrupted
// array_buffer, it would target the substituted buffer instead of the real
// (shared) memory, and a waiter parked on the real memory would never be
// woken. The worker (set up above, before the corruption) waits on the real
// memory through its own, uncorrupted WasmMemoryObject; notify it from the
// main thread's corrupted instance and expect the wait to resolve with "ok"
// rather than time out.
(function TestNotifyWakesRealWaiter() {
  const k10sNs = 10_000_000_000n;
  worker.postMessage({action: 'wait32', arguments: [0, 0x11223344, k10sNs]});

  const deadline = performance.now() + 10_000;
  let notified;
  while ((notified = notify32(0, 1)) === 0) {
    if (performance.now() > deadline) {
      throw new Error('Could not notify worker within 10s');
    }
  }
  assertEquals(1, notified);
  assertEquals(kAtomicWaitOk, worker.getMessage());

  worker.terminate();
})();

print('RESULT: done');
