// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/harmony/atomics-waitasync-helpers.js");

function workerCode() {
  onmessage = function() {
    const sab = new SharedArrayBuffer(16);
    const i32a = new Int32Array(sab);

    // Create a waiter with a timeout.
    const result = Atomics.waitAsync(i32a, 0, 0, 1);

    result.value.then(
      (value) => { postMessage("result " + value); },
      () => { postMessage("unexpected"); });
  };
}

const expected_messages = [
  "result timed-out"
];

runTestWithWorker(workerCode, expected_messages);
