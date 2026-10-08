// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-struct

"use strict";

(function TestRegress524783017() {
  function workerCode() {
    onmessage = function({data: {mutex, box}}) {
      while (Atomics.load(box, 0) === 0) {
        Atomics.Mutex.lockWithTimeout(mutex, () => {}, 1);
      }
    };
  }

  let box = new Int32Array(new SharedArrayBuffer(4));
  let mutex = new Atomics.Mutex();
  let workers = [];
  for (let i = 0; i < 16; i++) {
    let w = new Worker(workerCode, { type: 'function' });
    w.postMessage({ mutex, box });
    workers.push(w);
  }
  let start = Date.now();
  while (Date.now() - start < 200) {}
  Atomics.store(box, 0, 1);
  for (let w of workers) w.terminate();
})();
