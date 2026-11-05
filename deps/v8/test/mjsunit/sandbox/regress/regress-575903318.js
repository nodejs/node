// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const extension = buffer =>
    Sandbox.getAddressOf(buffer) +
    Sandbox.getFieldOffset(
        Sandbox.getInstanceTypeIdFor('JS_ARRAY_BUFFER_TYPE'), 'extension');
const scavenge = () => gc({type: 'minor'});

scavenge();
scavenge();
const old = new ArrayBuffer(8);
scavenge();
scavenge();
// The next handle of the old table space.
const next = mem.getUint32(extension(old), true) + (1 << 6);

const P = new ArrayBuffer(8, {maxByteLength: 1 << 20});
scavenge();
(() => {
  P.s = new ArrayBuffer(8);
  mem.setUint32(extension(P.s), next, true);
})();
for (let i = 0; i < 12000; i++) new ArrayBuffer(8);
// Promotes P, then copies P.s.
scavenge();
P.s = null;
for (let i = 0; i < 8000; i++) new ArrayBuffer(8);
scavenge();
for (let i = 0; i < 20000; i++) P.resize((i & 1) << 19);
