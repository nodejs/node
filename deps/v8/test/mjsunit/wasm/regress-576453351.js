// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const kBigGrow = 1024;
const kMaxPages = kBigGrow + 1;
const kNumRounds = 8;

const builder = new WasmModuleBuilder();
builder.addImportedMemory('m', 'memory', 1, kMaxPages, true);
builder.addFunction('load', kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32LoadMem8U, 0, 0])
    .exportFunc();
builder.addFunction('store', kSig_v_ii)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32StoreMem8, 0, 0])
    .exportFunc();
builder.addFunction('size', kSig_i_v)
    .addBody([kExprMemorySize, 0])
    .exportFunc();
const module = builder.toModule();

function workerCode() {
  onmessage = function({data: [memory, sync, bigGrow]}) {
    const flags = new Int32Array(sync);
    const arg = {
      valueOf() {
        Atomics.store(flags, 0, 1);
        while (Atomics.load(flags, 1) === 0) {}
        return bigGrow;
      }
    };
    try {
      memory.grow(arg);
      postMessage(true);
    } catch (e) {
      postMessage(e instanceof RangeError ? false : 'unexpected: ' + e);
    }
  };
}

const worker = new Worker(workerCode, {type: 'function'});
const sync = new SharedArrayBuffer(8);
const flags = new Int32Array(sync);
const mainArg = {
  valueOf() {
    while (Atomics.load(flags, 0) === 0) {}
    Atomics.store(flags, 1, 1);
    return 1;
  }
};

for (let round = 0; round < kNumRounds; ++round) {
  let memory;
  try {
    memory = new WebAssembly.Memory(
        {initial: 1, maximum: kMaxPages, shared: true});
  } catch (e) {
    // Out of memory on 32-bit platforms or memory-constrained bots.
    assertInstanceof(e, RangeError);
    break;
  }

  Atomics.store(flags, 0, 0);
  Atomics.store(flags, 1, 0);
  worker.postMessage([memory, sync, kBigGrow]);

  let smallGrowSucceeded = false;
  try {
    assertEquals(1, memory.grow(mainArg));
    smallGrowSucceeded = true;
  } catch (e) {
    assertInstanceof(e, RangeError);
  }
  const bigGrowSucceeded = worker.getMessage();
  assertEquals('boolean', typeof bigGrowSucceeded);

  if (smallGrowSucceeded) {
    assertFalse(bigGrowSucceeded);
    const {load, store, size} =
        new WebAssembly.Instance(module, {m: {memory}}).exports;
    assertEquals(2, size());
    assertEquals(2 * kPageSize, memory.buffer.byteLength);
    const outOfBoundsOffset = 2 * kPageSize + 4;
    assertTraps(kTrapMemOutOfBounds, () => store(outOfBoundsOffset, 0x5a));
    assertTraps(kTrapMemOutOfBounds, () => load(outOfBoundsOffset));
    assertEquals(2, memory.grow(1));
    assertEquals(0, load(outOfBoundsOffset));
  } else if (bigGrowSucceeded) {
    assertEquals(kMaxPages * kPageSize, memory.buffer.byteLength);
  } else {
    // Out of memory or reduced capacity on 32-bit platforms.
    assertEquals(kPageSize, memory.buffer.byteLength);
    break;
  }
}

worker.terminate();
