// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let targetMemory =
    new WebAssembly.Memory({initial: 1, shared: true, maximum: 1000});

const kNumWorkers = 2;
const kMaxGrows = 50;

// Use the first few bytes of the shared memory for control.
// control[0]: workers ready count.
// control[1]: start flag.
// control[2]: total grows count.
// control[3]: workers finished count.
let control = new Int32Array(targetMemory.buffer, 0, 4);

let workerCode = `
  onmessage = function(event) {
    let {targetMemory, kMaxGrows} = event.data;
    let control = new Int32Array(targetMemory.buffer, 0, 4);
    // Signal ready.
    Atomics.add(control, 0, 1);
    // Wait for start signal.
    while (Atomics.load(control, 1) === 0) {
    }
    for (let i = 0; i < kMaxGrows; ++i) {
      let b = targetMemory.buffer;
      targetMemory.grow(0);
      targetMemory.grow(1);
    }
    // Signal finished.
    Atomics.add(control, 3, 1);
  }
`;

for (let i = 0; i < kNumWorkers; i++) {
  new Worker(workerCode, {
    type: 'string'
  }).postMessage({targetMemory, kMaxGrows});
}

// Wait for all workers to be ready.
while (Atomics.load(control, 0) < kNumWorkers) {
}

// Start workers.
Atomics.store(control, 1, 1);

// Wait for all workers to finish.
while (Atomics.load(control, 3) < kNumWorkers) {
}
