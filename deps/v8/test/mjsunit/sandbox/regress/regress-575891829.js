// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const kExtension = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('JS_ARRAY_BUFFER_TYPE'), 'extension');

const resizable = {maxByteLength: 1 << 20};
const old = new ArrayBuffer(8, resizable);
// Move to old space.
gc();
const handle = mem.getUint32(Sandbox.getAddressOf(old) + kExtension, true);

// Where the second thread writes.
const cell = new Uint32Array(new SharedArrayBuffer(4));
new Worker(
    `
  const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
  onmessage = event => {
    const cell = new Uint32Array(event.data);
    for (;;) {
      const address = Atomics.load(cell, 0);
      if (address) mem.setUint32(address, ${handle}, true);
    }
  };`,
    {type: 'string'})
    .postMessage(cell.buffer);

// Work for the sweeping job.
for (let i = 0; i < 8000; i++) new ArrayBuffer(8);
for (let i = 0; i < 10; i++) {
  const probe = new ArrayBuffer(8);
  // The next buffer's field.
  const next =
      Sandbox.getAddressOf(probe) + Sandbox.getSizeOf(probe) + kExtension;
  Atomics.store(cell, 0, next);
  while (mem.getUint32(next, true) !== handle) {
  }
  new ArrayBuffer(8, resizable);
  Atomics.store(cell, 0, 0);
}

gc({type: 'minor'});
for (let i = 0; i < 10000; i++) old.resize((i & 1) << 19);
