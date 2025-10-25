// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function workerScript() {
  onmessage = function(e) {
    const gsab = e.data.gsab;
    const newSize = e.data.newSize;
    const sync = new Int32Array(e.data.sync);
    Atomics.store(sync, 0, 1);
    Atomics.notify(sync, 0);
    Atomics.wait(sync, 0, 1);
    gsab.grow(newSize);
    postMessage("done");
  };
}

function main() {
  // Large sizes to increase memcpy time and OOB impact.
  const initialSize = 512 * 1024;
  const maxSize = 2 * initialSize;
  const newSize = maxSize;
  const syncSAB = new SharedArrayBuffer(4);
  const sync = new Int32Array(syncSAB);
  // States: 0=init, 1=worker ready, 2=main ready (grow now)
  for (let i = 0; i < 5; i++) {
    // Reset synchronization state.
    Atomics.store(sync, 0, 0);
    let gsab_i = new SharedArrayBuffer(initialSize, { maxByteLength: maxSize });
    const ta_i = new Int8Array(gsab_i);
    const worker = new Worker(workerScript, { type: 'function' });
    worker.postMessage({ gsab: gsab_i, newSize: newSize, sync: syncSAB });
    // Wait for worker to become ready.
    while (Atomics.load(sync, 0) !== 1) {}
    // Signal worker to resize buffer.
    Atomics.store(sync, 0, 2);
    Atomics.notify(sync, 0);
    ta_i.sort();
    worker.getMessage();
    worker.terminate();
  }
}
main();
