// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --js-staging

d8.file.execute('test/mjsunit/typedarray-helpers.js');
d8.file.execute('test/mjsunit/huge-typedarrays-helpers.js');

// Test atomics.
// Create one big shared ArrayBuffer.
const length = 21 * GB;
// Skip if the platform does not support such big ArrayBuffers.
let sab = length > kMaxArrayBufferByteLength
    ? undefined
    : ignoreOOM(() => new SharedArrayBuffer(length));

if (sab) {
  let int32_arr = new Int32Array(sab);
  const num_int32_elems = length / Int32Array.BYTES_PER_ELEMENT;
  TestAtomicsOperations(int32_arr, 13);
  TestAtomicsOperations(int32_arr, num_int32_elems - 13);
  AssertAtomicsOperationsThrow(int32_arr, num_int32_elems, RangeError);

  // Test wait / notify.
  let worker = new Worker(function() {
    onmessage = function({data:msg}) {
      if (msg.action == 'wait') {
        let ta = new Int32Array(msg.buf);
        postMessage(Atomics.wait(ta, msg.index, msg.value, msg.timeout));
        return;
      }
      postMessage(`Unknown action: ${msg.data.action}`);
    }
  }, {type: 'function'});

  for (let index of [3, num_int32_elems - 3]) {
    worker.postMessage(
        {action: 'wait', buf: sab, index: index, value: 0, timeout: 100});
    assertEquals('timed-out', worker.getMessage());

    worker.postMessage(
        {action: 'wait', buf: sab, index: index, value: 1, timeout: 100});
    assertEquals('not-equal', worker.getMessage());

    worker.postMessage(
        {action: 'wait', buf: sab, index: index, value: 0, timeout: 10000});
    let timeout = performance.now() + 10000;
    while (true) {
      let woken = Atomics.notify(int32_arr, index, 1);
      if (woken == 1) break;
      assertEquals(0, woken);
      if (performance.now() > timeout) throw new Error('could not wake');
    }
    assertEquals('ok', worker.getMessage());
  }
}
