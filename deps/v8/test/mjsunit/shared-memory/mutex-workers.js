// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-struct --allow-natives-syntax

"use strict";

if (this.Worker) {

(function TestMutexWorkers() {
  let workerScript =
      `onmessage = function(msg) {
         let mutex = msg.mutex;
         let box = msg.box;
         for (let i = 0; i < 10; i++) {
           Atomics.Mutex.lock(mutex, function() {
             box.counter++;
           });
         }
         postMessage("done");
       };
       postMessage("started");`;

  let worker1 = new Worker(workerScript, { type: 'string' });
  let worker2 = new Worker(workerScript, { type: 'string' });
  assertEquals("started", worker1.getMessage());
  assertEquals("started", worker2.getMessage());

  let Box = new SharedStructType(['counter']);
  let box = new Box();
  box.counter = 0;
  let mutex = new Atomics.Mutex();
  let msg = { mutex, box };
  worker1.postMessage(msg);
  worker2.postMessage(msg);
  assertEquals("done", worker1.getMessage());
  assertEquals("done", worker2.getMessage());
  assertEquals(20, box.counter);

  worker1.terminate();
  worker2.terminate();
})();

}
