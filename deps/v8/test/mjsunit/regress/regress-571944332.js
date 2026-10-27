// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let workerCode = `
  onmessage = function() {
    const maxByteLength = Math.min((%ArrayBufferMaxByteLength() + 1) / 2, 2**42) / 128;
    let bufs = [];
    while (true) {
      try {
        bufs.push(new ArrayBuffer(0, {maxByteLength}));
      } catch(e) {
        bufs.length = 0;
      }
    }
  };
`;

let worker = new Worker(workerCode, {type: 'string'});
worker.postMessage('start');

const maxByteLength = Math.min((%ArrayBufferMaxByteLength() + 1) / 2, 2 ** 42);

let rabs = [];
for (let i = 0; i < 100; i++) {
  try {
    rabs.push(new ArrayBuffer(0, {maxByteLength}));
  } catch (e) {
    break;
  }
}

try {
  for (let iter = 0; iter < 1000; iter++) {
    for (let i = 0; i < rabs.length; i++) {
      rabs[i] = rabs[i].transfer(0);
    }
  }
} catch (e) {
  assertInstanceof(e, RangeError);
}

worker.terminate();
