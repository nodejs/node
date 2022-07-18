// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --shared-string-table --harmony-struct --allow-natives-syntax

'use strict';

if (this.Worker) {
  (function TestSharedArrayPostMessage() {
    let workerScript = `onmessage = function(arr) {
         // Non-atomic write that will be made visible once main thread
         // observes the atomic write below.
         arr[0][0] = 42;
         arr[1].payload = 84;
         Atomics.store(arr, 2, "worker");
       };
       postMessage("started");`;

    let worker = new Worker(workerScript, {type: 'string'});
    let started = worker.getMessage();
    assertEquals('started', started);

    let OuterArray = new SharedArray(3);
    let Struct = new SharedStructType(['payload']);
    OuterArray[0] = new SharedArray(1);
    OuterArray[1] = new Struct();
    OuterArray[2] = 'main';
    assertEquals(undefined, OuterArray[0][0]);
    assertEquals(undefined, OuterArray[1].payload);
    assertEquals('main', OuterArray[2]);
    worker.postMessage(OuterArray);
    // Spin until we observe the worker's write on index 2.
    while (Atomics.load(OuterArray, 2) !== 'worker') {
    }
    // The non-atomic store write must also be visible.
    assertEquals(42, OuterArray[0][0]);
    assertEquals(84, OuterArray[1].payload);

    worker.terminate();
  })();
}
