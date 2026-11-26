// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-memory-corruption-api --expose-gc

const kChunkMask = ~0x3ffff;
const kBlackAllocatedMask = 1 << 6;

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));

// Allocate two ~100 KB arrays and promote them to OLD_SPACE so the second
// backing store lands on a new OLD_SPACE page with its remaining free space
// linked in old_space()->free_list().
let a = [
  new Array(25000).fill(0),
  new Array(25000).fill(0),
];
gc({type: 'minor'});
gc({type: 'minor'});

const targetPage = Sandbox.readObjectField(a[1], 'elements') & kChunkMask;
a = null;

// Set MemoryChunk::BLACK_ALLOCATED in the untrusted chunk header of targetPage
// without setting the trusted BasePage flag, then trigger a Major GC.
mem.setUint32(
    targetPage, mem.getUint32(targetPage, true) | kBlackAllocatedMask, true);
gc();
