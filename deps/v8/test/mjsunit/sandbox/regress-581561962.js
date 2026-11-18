// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

Error.stackTraceLimit = 0;

const getAddress = Sandbox.getAddressOf;
const getSize = Sandbox.getSizeOf;
const numberToString = Number.prototype.toString;

// 2^32 + 16384 (high word 0x41f00004) and -(2^32 - 65536) (high word
// 0xc1efffe0) are both outside Smi range, so ToInteger_Inline allocates a
// normalized HeapNumber in the sandbox. Toggling the high word between these
// two values across the separate `radixNumber < 2` and `radixNumber > 36`
// checks lets the out-of-range radix bypass the [2, 36] bounds check while
// TruncateHeapNumberValueToWord32 truncates it to a positive uint32 (16384 or
// 65536).
const kOutOfSmiRangeRadix = 4294983680;
const kPositiveRadixHighWord = 0x41f00004;
const kNegativeRadixHighWord = 0xc1efffe0;

const sharedBuffer = new SharedArrayBuffer(16);
const syncState = new Int32Array(sharedBuffer);
const warmupTarget = {};
syncState[0] = (getAddress(warmupTarget) + 8) >>> 2;

const racerWorker = new Worker(`
  const sandboxHeap = new Uint32Array(new Sandbox.MemoryView(0, 0x100000000));
  function toggle(highWordIndex) {
    for (let i = 0; i < 200; ++i) {
      sandboxHeap[highWordIndex] = ${kPositiveRadixHighWord};
      sandboxHeap[highWordIndex] = ${kNegativeRadixHighWord};
    }
  }
  onmessage = function(msg) {
    const syncState = new Int32Array(msg.data);
    const warmupIndex = Atomics.load(syncState, 0);
    for (let w = 0; w < 100; ++w) {
      toggle(warmupIndex);
    }
    postMessage("ready");
    while (Atomics.load(syncState, 1) === 0) {
      toggle(Atomics.load(syncState, 0));
    }
  };
`, {type: 'string'});

racerWorker.postMessage(sharedBuffer);
assertEquals('ready', racerWorker.getMessage());

let bypassed = false;
try {
  for (let attempt = 0; attempt < 200; ++attempt) {
    // Allocate a young-generation marker object so the HeapNumber allocated by
    // ToInteger_Inline(kOutOfSmiRangeRadix) is placed immediately after it,
    // with its float64 high word at offset +8.
    const marker = {};
    const radixHighWordIndex = (getAddress(marker) + getSize(marker) + 8) >>> 2;
    Atomics.store(syncState, 0, radixHighWordIndex);
    try {
      numberToString.call(15, kOutOfSmiRangeRadix);
      bypassed = true;
      break;
    } catch (e) {
    }
  }
} finally {
  Atomics.store(syncState, 1, 1);
  racerWorker.terminate();
}
assertFalse(bypassed);
