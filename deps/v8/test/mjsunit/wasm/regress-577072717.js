// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kPageSize = 65536;
const kNumRounds = 20;
const memory = new WebAssembly.Memory(
    {initial: 0, maximum: 5 * kNumRounds, shared: true});
const gsab = memory.toResizableBuffer();
const sync = new Int32Array(new SharedArrayBuffer(8));

const worker = new Worker(function() {
  onmessage = function({data: [gsab, sync, numRounds]}) {
    for (let i = 1; i <= numRounds; ++i) {
      Atomics.store(sync, 0, i);
      while (Atomics.load(sync, 1) !== i) {}
      gsab.grow(4 * i * 65536);
      postMessage(true);
    }
  };
}, {type: 'function'});

worker.postMessage([gsab, sync, kNumRounds]);

for (let i = 1; i <= kNumRounds; ++i) {
  while (Atomics.load(sync, 0) !== i) {}
  Atomics.store(sync, 1, i);
  try {
    gsab.grow((4 * i - 3) * kPageSize);
  } catch (e) {
    assertInstanceof(e, RangeError);
  }
  assertTrue(worker.getMessage());
  assertEquals(4 * i * kPageSize, gsab.byteLength);
}

worker.terminate();
