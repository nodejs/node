// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc --no-verify-heap --disable-optimizing-compilers

const kPageSize = 1 << 18;
const kPageMask = ~(kPageSize - 1);
const kPointersToHereAreInteresting = 1 << 1;
const kPointersFromHereAreInteresting = 1 << 2;
const kInterestingPointersMask =
    kPointersToHereAreInteresting | kPointersFromHereAreInteresting;
const kBufferOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('JS_TYPED_ARRAY_TYPE'), 'byte_offset') - 8;
const kBackingStoreOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('JS_ARRAY_BUFFER_TYPE'), 'extension') - 8;

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));

function getBackingStorePtr(ab) {
  return mem.getBigUint64(Sandbox.getAddressOf(ab) + kBackingStoreOffset, true);
}

const views = [];
for (let i = 0; i < 100000; i++) {
  views.push(new Uint8Array(8));
}
gc();

const kStartIndex = 10000;
const pages = new Set();
for (let i = kStartIndex; i < views.length; i++) {
  const addr = Sandbox.getAddressOf(views[i]);
  pages.add((mem.getUint32(addr + kBufferOffset, true) & kPageMask) >>> 0);
}
const [page0] = pages;

// Allocate in OldSpace until incremental marking starts, then spin briefly so
// concurrent marking marks the promoted views and their unmaterialized
// JSArrayBuffers black before their ArrayBufferExtensions are created.
const ballast = [];
while (!(mem.getUint32(page0, true) & kPointersToHereAreInteresting)) {
  ballast.push(new Array(4000).fill(0));
}
for (let i = 0; i < 5e7; i++) {}

// Materialize the ArrayBufferExtensions while the untrusted barrier flags are
// cleared on the host pages to verify WriteBarrier::ForArrayBufferExtension
// does not rely on them.
const savedFlags = new Map();
for (const page of pages) {
  const flags = mem.getUint32(page, true);
  savedFlags.set(page, flags);
  mem.setUint32(page, flags & ~kInterestingPointersMask, true);
}
for (let i = kStartIndex; i < views.length; i++) {
  views[i].buffer;
}
for (const page of pages) {
  mem.setUint32(page, savedFlags.get(page), true);
}

// Finalize incremental marking and launch concurrent ArrayBufferSweeper on old_.
gc({type: 'minor'});

// Track the background sweeper wavefront using whether transfer() reuses the
// existing BackingStore (not yet swept -> step by 1) or allocates a new one
// because the EPT entry was zapped (already swept -> hop ahead by 32).
for (let k = kStartIndex; k < views.length;) {
  const ab = views[k].buffer;
  const oldPtr = getBackingStorePtr(ab);
  const out = ab.transfer();
  const newPtr = getBackingStorePtr(out);
  k += (oldPtr === newPtr) ? 1 : 32;
}
