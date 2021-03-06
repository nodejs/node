// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test is a reproduction of a crash that happens when a TypedArray
// backed by a SharedArrayBuffer is concurrently modified while sorting.
// Segfaults would need a long time to trigger in normal builds, so this
// reproduction is tailored to trigger on ASAN builds. On ASAN builds,
// out-of-bounds accesses while sorting would result in an immediate failure.

const lock = new Int32Array(new SharedArrayBuffer(4));

const kIterations = 5000;
const kLength = 2000;

const kStageIndex = 0;
const kStageInit = 0;
const kStageRunning = 1;
const kStageDone = 2;

Atomics.store(lock, kStageIndex, kStageInit);

function WaitUntil(expected) {
  while (true) {
    const value = Atomics.load(lock, kStageIndex);
    if (value === expected) break;
  }
}

const workerScript = `
  onmessage = function([sab, lock]) {
    const i32a = new Int32Array(sab);
    Atomics.store(lock, ${kStageIndex}, ${kStageRunning});

    for (let j = 1; j < ${kIterations}; ++j) {
      for (let i = 0; i < i32a.length; ++i) {
        i32a[i] = j;
      }
    }

    postMessage("done");
    Atomics.store(lock, ${kStageIndex}, ${kStageDone});
  };`;

const worker = new Worker(workerScript, {type: 'string'});

const i32a = new Int32Array(
  new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * kLength)
);

worker.postMessage([i32a.buffer, lock]);
WaitUntil(kStageRunning);

for (let i = 0; i < kIterations; ++i) {
  i32a.sort();
}

WaitUntil(kStageDone);
assertEquals(worker.getMessage(), "done");
