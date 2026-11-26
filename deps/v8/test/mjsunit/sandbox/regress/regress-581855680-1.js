// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Offset 0x40000000 is 1 above kSmiMaxValue (0x3fffffff), so UintPtr53ToNumber
// boxes the pre-checked offset into a mutable in-sandbox HeapNumber.
const kValidOffset = 0x40000000;
const kForgedOffset = 0x15000000000;  // 1 TiB + 320 GiB (past sandbox + guard).
const kUseI64 = globalThis.kUseI64 ?? false;

// 1. Verify that WasmAtomicWait reads the trusted BackingStore (and sharedness)
// from WasmTrustedInstanceData rather than the untrusted WasmMemoryObject.
(function testCorruptedMemoryBackingStore() {
  const mem1 = new WebAssembly.Memory({initial: 2, maximum: 2, shared: true});
  const mem2 = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
  const unshared = new WebAssembly.Memory({initial: 2, maximum: 2});
  new BigInt64Array(mem2.buffer)[0] = 1n;

  const builder = new WasmModuleBuilder();
  builder.addImportedMemory('m', 'mem', 2, 2, true);
  if (kUseI64) {
    builder.addFunction('waitAt', kSig_i_i).addBody([
      kExprLocalGet, 0,
      kExprI64Const, 1,
      kExprI64Const, 0,
      kAtomicPrefix, kExprI64AtomicWait, 3, 0,
    ]).exportFunc();
  } else {
    builder.addFunction('waitAt', kSig_i_i).addBody([
      kExprLocalGet, 0,
      kExprI32Const, 1,
      kExprI64Const, 0,
      kAtomicPrefix, kExprI32AtomicWait, 2, 0,
    ]).exportFunc();
  }
  const instance = builder.instantiate({m: {mem: mem1}});
  const origBackingStore =
      Sandbox.readObjectField(mem1, 'managed_backing_store');

  Sandbox.corruptObjectField(
      mem1, 'managed_backing_store',
      Sandbox.readObjectField(mem2, 'managed_backing_store'));
  assertEquals(kAtomicWaitNotEqual, instance.exports.waitAt(0));
  assertEquals(kAtomicWaitNotEqual, instance.exports.waitAt(0x10000));

  Sandbox.corruptObjectField(
      mem1, 'managed_backing_store',
      Sandbox.readObjectField(unshared, 'managed_backing_store'));
  assertEquals(kAtomicWaitNotEqual, instance.exports.waitAt(0));

  Sandbox.corruptObjectField(mem1, 'managed_backing_store', origBackingStore);
})();

// 2. Verify that concurrently mutating the boxed HeapNumber offset cannot cause
// an out-of-bounds wait outside the sandbox.
function workerMain(useI64, validOffset) {
  d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

  const kPages = (validOffset >>> 16) + 1;

  const builder = new WasmModuleBuilder();
  builder.addMemory(kPages, kPages, true);
  if (useI64) {
    builder.addFunction('wait', kSig_i_v).addBody([
      kExprI32Const, ...wasmSignedLeb(validOffset),
      kExprI64Const, 1,
      kExprI64Const, 0,
      kAtomicPrefix, kExprI64AtomicWait, 3, 0,
    ]).exportFunc();
  } else {
    builder.addFunction('wait', kSig_i_v).addBody([
      kExprI32Const, ...wasmSignedLeb(validOffset),
      kExprI32Const, 1,
      kExprI64Const, 0,
      kAtomicPrefix, kExprI32AtomicWait, 2, 0,
    ]).exportFunc();
  }

  let instance;
  try {
    instance = builder.instantiate();
  } catch (e) {
    if (e instanceof RangeError && e.message.includes('Out of memory')) {
      postMessage({oom: true});
      return;
    }
    throw e;
  }
  const wait = instance.exports.wait;

  // Warm up lazy compilation and calibrate the HeapNumber payload offset
  // relative to a preceding young-space object allocation.
  wait();
  const calibrations = [];
  const keepAlive = [];
  for (let i = 0; i < 16; ++i) {
    const before = {};
    const start = Sandbox.getAddressOf(before) + Sandbox.getSizeOf(before);
    wait();
    const after = {};
    const end = Sandbox.getAddressOf(after);
    calibrations.push({start, end});
    keepAlive.push(before, after);
  }
  globalThis.keepAlive = keepAlive;
  postMessage({calibrations});

  onmessage = function({data: sharedBuffer}) {
    if (!sharedBuffer) {
      postMessage({done: true});
      return;
    }
    const state = new Int32Array(sharedBuffer);
    postMessage({started: true});
    for (let i = 1; i <= 5000; ++i) {
      const marker = {};
      const allocStart =
          Sandbox.getAddressOf(marker) + Sandbox.getSizeOf(marker);
      // Store `allocStart >>> 2` so the value is always a PositiveSmi and
      // `Atomics.store` never allocates a HeapNumber before `wait()`.
      Atomics.store(state, 0, allocStart >>> 2);
      Atomics.store(state, 1, i);
      while (Atomics.load(state, 3) !== i) {}
      wait();
    }
    Atomics.store(state, 2, 1);
    postMessage({done: true});
  };
}

const worker = new Worker(
    `(${workerMain})(${kUseI64}, ${kValidOffset});`, {type: 'string'});
const init = worker.getMessage();
if (!init.oom) {
  const sandboxMemory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
  const deltaCounts = new Map();
  for (const c of init.calibrations) {
    if (c.end <= c.start || c.end - c.start > 0x1000) continue;
    for (let off = c.start; off + 8 <= c.end; off += 4) {
      if (sandboxMemory.getFloat64(off, true) === kValidOffset) {
        const delta = off - c.start;
        deltaCounts.set(delta, (deltaCounts.get(delta) || 0) + 1);
        break;
      }
    }
  }
  let payloadDelta = -1;
  let bestCount = 0;
  for (const [delta, count] of deltaCounts) {
    if (count > bestCount) {
      payloadDelta = delta;
      bestCount = count;
    }
  }
  if (payloadDelta < 0) {
    // Under extreme GC stress (e.g. --gc-interval=1), bump-pointer calibration
    // may not observe contiguous allocations.
    worker.postMessage(null);
    worker.getMessage();
  } else {
    // [allocStartShifted, generation, done, ackGeneration]
    const sharedBuffer = new SharedArrayBuffer(16);
    const state = new Int32Array(sharedBuffer);
    worker.postMessage(sharedBuffer);
    worker.getMessage();

    let ack = 0;
    while (Atomics.load(state, 2) === 0) {
      const allocStart = (Atomics.load(state, 0) << 2) >>> 0;
      if (allocStart !== 0) {
        const addr = allocStart + payloadDelta;
        if (addr + 7 <= 0xffffffff &&
            ((allocStart - 4) >>> 18) === ((addr + 7) >>> 18)) {
          sandboxMemory.setFloat64(addr, kForgedOffset, true);
          sandboxMemory.setFloat64(addr, kForgedOffset, true);
          sandboxMemory.setFloat64(addr, kForgedOffset, true);
          sandboxMemory.setFloat64(addr, kForgedOffset, true);
        }
        const gen = Atomics.load(state, 1);
        if (gen !== 0 && gen !== ack) {
          Atomics.store(state, 3, gen);
          ack = gen;
        }
      }
    }
    worker.getMessage();
  }
}
worker.terminateAndWait();
