// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const sz = 0x100000000 + 64 * 1024; // 4GB + 64Kb
let sab;
try {
  sab = new SharedArrayBuffer(sz);
} catch (e) {
  // We must be running in an 32-bit environment.
  quit(0);
}

let rab = new ArrayBuffer(16, { maxByteLength: sz });
let gsab = new SharedArrayBuffer(32, { maxByteLength: sz });

const viewLen = 256;
let lowView = new Uint8Array(sab, 0, viewLen);
let highView = new Uint8Array(sab, 0x100000000, viewLen);

for (let i = 0; i < viewLen; i++) {
  lowView[i] = 0xaa + i;
  highView[i] = 0xbb + i;
}

const workerScript = function () {
  onmessage = function ({ data: msg }) {
    postMessage({
      highView: msg.highView,
      lowView: msg.lowView,
      rab: msg.rab,
      gsab: msg.gsab,
    });
  };
};

const worker = new Worker(workerScript, { type: "function" });

worker.postMessage({
  highView: highView,
  lowView: lowView,
  rab: rab,
  gsab: gsab,
});
let response = worker.getMessage();
let lowView2 = response.lowView;
let highView2 = response.highView;
let rab2 = response.rab;
let gsab2 = response.gsab;

assertEquals(lowView.byteOffset, lowView2.byteOffset);
assertEquals(lowView.byteLength, lowView2.byteLength);
assertEquals(highView.byteOffset, highView2.byteOffset);
assertEquals(highView.byteLength, highView2.byteLength);
for (let i = 0; i < viewLen; i++) {
  assertEquals(lowView[i], lowView2[i]);
  assertEquals(highView[i], highView2[i]);
}

assertEquals(rab.byteLength, rab2.byteLength);
assertEquals(rab.maxByteLength, rab2.maxByteLength);

assertEquals(gsab.byteLength, gsab2.byteLength);
assertEquals(gsab.maxByteLength, gsab2.maxByteLength);

worker.terminate();
