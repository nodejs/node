// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const sz = 0x100000000 + 64 * 1024; // 4GB + 64Kb
let buf;
try {
  buf = new ArrayBuffer(sz);
} catch (e) {
  // We must be running in an 32-bit environment.
  quit(0);
}

const viewLen = 256;
let view = new Uint8Array(buf, 0x100000000, viewLen);

for (let i = 0; i < viewLen; i++) {
  view[i] = 0xaa + i;
}

const workerScript = function () {
  onmessage = function ({ data: msg }) {
    try {
      // Make sure that transferring of buffers from this side also works.
      postMessage({ view: msg.view, buf: msg.buf }, [msg.buf]);
    } catch (e) {
      postMessage({ error: e });
    }
  };
};

const worker = new Worker(workerScript, { type: "function" });

worker.postMessage({ view: view, buf: buf }, [buf]);
let response = worker.getMessage();
if (response.error) throw response.error;
let view2 = response.view;
let buf2 = response.buf;

assertEquals(view2.byteLength, viewLen);
assertEquals(view2.byteOffset, 0x100000000);
for (let i = 0; i < viewLen; i++) {
  assertEquals(view2[i], (0xaa + i) & 0xff);
}

assertEquals(buf2.byteLength, sz);
worker.terminate();
