// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-base-64 --allow-natives-syntax

const workerScript = `
  onmessage = function(event) {
    const sab = event.data.buffer;
    const uint8Array = new Uint8Array(sab);

    const dataToWrite = [102, 111, 111, 98, 97, 114, 255, 255];

    for (let i = 0; i < dataToWrite.length; ++i) {
        uint8Array[i] = dataToWrite[i];
    }

    postMessage("started");

    while (true) {
      for (let i = 0; i < dataToWrite.length; ++i) {
        uint8Array[i] = dataToWrite[i];
      }
    }
  };
`;

function testConcurrentSharedArrayBufferUint8ArraySetFromHex() {
  const sab = new SharedArrayBuffer(8);
  const uint8ArrayMain = new Uint8Array(sab);

  // Create a worker
  const worker = new Worker(workerScript, {type: 'string'});

  // Send the SharedArrayBuffer
  worker.postMessage({buffer: sab});
  assertEquals('started', worker.getMessage());

  // Give the worker a little time to write
  for (let i = 0; i < 10000; ++i) {
  }

  // Call setFromHex on the main thread's view of the SAB
  for (let i = 0; i < 100; i++) {
    const result = uint8ArrayMain.setFromHex('666f6f626172');

    const actual = Array.from(uint8ArrayMain);

    assertEquals(
        actual, [102, 111, 111, 98, 97, 114, 255, 255],
        'setFromHex result mismatch with concurrent writes');
  }

  // Terminate the worker (now it should exit its loop)
  worker.terminate();
}

// Run the test function
testConcurrentSharedArrayBufferUint8ArraySetFromHex();
