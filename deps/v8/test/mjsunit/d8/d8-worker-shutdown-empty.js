// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-runs=1

const kBatchSize = 10;
const kNumBatches = 10;

function RunWorkerBatch(count) {
  let script = `postMessage(42)`;

  // Launch workers.
  let workers = new Array(count);
  for (let i = 0; i < count; i++) {
    workers[i] = new Worker(script, {type : 'string'});
  }

  // Terminate half of the workers early.
  for (let i = 0; i < workers.length; i++) {
    if ((i & 1) == 1) workers[i].terminate();
  }

  // Get messages from some workers.
  for (let i = 0; i < workers.length; i++) {
    let msg = workers[i].getMessage();
    assertTrue(msg === undefined || msg === 42);
    // terminate all workers.
    workers[i].terminate();
  }
}

(function RunTest() {
  print(`running ${kNumBatches} batches...`);
  let time = performance.now();
  for (let i = 0; i < kNumBatches; i++) {
    let before = performance.now();
    RunWorkerBatch(kBatchSize);
    let time = performance.now() - before;
    print(`batch ${i+1}, Δ = ${(time).toFixed(3)} ms`);
  }
})();
