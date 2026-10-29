// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/harmony/atomics-waitasync-helpers.js");

function workerCode() {
  const sab = new SharedArrayBuffer(16);
  const i32a = new Int32Array(sab);

  onmessage = function() {
    // Create a waiter with a long timeout.
    const result_slow = Atomics.waitAsync(i32a, 0, 0, 200000);
    // Create a waiter with a short timeout.
    const result_fast = Atomics.waitAsync(i32a, 0, 0, 1);

    result_slow.value.then(
      (value) => { postMessage("slow " + value); },
      () => { postMessage("unexpected"); });

    result_fast.value.then(
      (value) => {
        postMessage("fast " + value);
        // Wake up the waiter with the long time out.
        const notify_return_value = Atomics.notify(i32a, 0, 1);
        postMessage("notify return value " + notify_return_value);
      },
      () => { postMessage("unexpected"); });
  };
}

const expected_messages = [
  "fast timed-out",
  "notify return value 1",
  "slow ok"
];

runTestWithWorker(workerCode, expected_messages);
