// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

d8.file.execute("test/mjsunit/harmony/atomics-waitasync-helpers.js");

function workerCode() {
  onmessage = function() {
    (function() {
      const sab = new SharedArrayBuffer(16);
      const i32a = new Int32Array(sab);
      // Create a waiter with a timeout.
      const result = Atomics.waitAsync(i32a, 0, 0, 1);
      result.value.then(
        (value) => { postMessage("result " + value); },
        () => { postMessage("unexpected"); });
      })();
    // Make sure sab, ia32 and result get gc()d.
    gc();

    // Even if the buffer went out of scope, we keep the waitAsync alive so that it can still time out.
    let resolved = false;
    const sab2 = new SharedArrayBuffer(16);
    const i32a2 = new Int32Array(sab2);
    const result2 = Atomics.waitAsync(i32a2, 0, 0);
    result2.value.then(
      (value) => { postMessage("result2 " + value); },
      () => { postMessage("unexpected"); });

    const notify_return_value = Atomics.notify(i32a2, 0);
    postMessage("notify return value " + notify_return_value);
  };
}

const expected_messages = [
  "notify return value 1",
  "result2 ok",
  "result timed-out"
];

runTestWithWorker(workerCode, expected_messages);
