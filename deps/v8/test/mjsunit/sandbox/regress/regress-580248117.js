// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc --allow-natives-syntax --no-verify-heap

const kChunkMask = ~0x3ffff;
const kYoungFlags = (1 << 3) | (1 << 4);  // MemoryChunk::FROM_PAGE | TO_PAGE
const kOptions = {maxByteLength: 32};

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const kExtension = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('JS_ARRAY_BUFFER_TYPE'), 'extension');
const pageOf = obj => (Sandbox.getAddressOf(obj) & kChunkMask) >>> 0;

const worker = new Worker(function() {
  onmessage = function({data: {pageOff, stripped}}) {
    const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));

    %BlockAt('JSArrayBuffer::CreateExtension', 10000);
    %BlockAt('SweepArrayBufferYoung', 10000);
    %BlockAt('ArrayBufferSweeper::Resize', 10000);
    postMessage('ready');

    // Clear young generation page flags between the two InYoungGeneration
    // checks in JSArrayBuffer::CreateExtension.
    %WaitUntilBlocked('JSArrayBuffer::CreateExtension', 10000);
    mem.setUint8(pageOff, stripped);
    %Resume('JSArrayBuffer::CreateExtension');

    // Wait until oldRab.resize(0) has loaded `extension` and reached
    // ArrayBufferSweeper::Resize, then let SweepYoung free `extension`.
    %WaitUntilBlocked('SweepArrayBufferYoung', 10000);
    %WaitUntilBlocked('ArrayBufferSweeper::Resize', 10000);
    %BlockAt('SweepArrayBufferYoungFinished', 10000);
    %Resume('SweepArrayBufferYoung');
    %WaitUntilBlocked('SweepArrayBufferYoungFinished', 10000);
    %Resume('SweepArrayBufferYoungFinished');
    %Resume('ArrayBufferSweeper::Resize');
  };
}, {type: 'function'});

(function() {
  // Keep a baseline ArrayBuffer alive so external memory accounting does not
  // underflow after young sweeping decrements dead bytes.
  const keepAlive = new ArrayBuffer(32);

  // Promote a resizable ArrayBuffer to old space to host the mismatched handle.
  const oldRab = new ArrayBuffer(16, kOptions);
  gc({type: 'minor'});
  gc({type: 'minor'});
  const oldAddr = Sandbox.getAddressOf(oldRab);

  const pageOff = pageOf({});
  const orig = mem.getUint8(pageOff);
  worker.postMessage({pageOff, stripped: orig & ~kYoungFlags});
  worker.getMessage();

  const rab = new ArrayBuffer(16, kOptions);
  mem.setUint8(pageOff, orig);

  const rabAddr = Sandbox.getAddressOf(rab);
  const h = mem.getUint32(rabAddr + kExtension, true);
  mem.setBigUint64(rabAddr + kExtension, 0n, true);

  gc({type: 'minor'});

  mem.setUint32(oldAddr + kExtension, h, true);
  oldRab.resize(0);
})();
