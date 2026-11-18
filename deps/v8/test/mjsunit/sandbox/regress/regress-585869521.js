// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

// ApplyOptionsToTag (src/objects/js-locale.cc) must not pass a UTF-8 length
// >= 2^31 to icu::LocaleBuilder::setLanguageTag(), where it is truncated to
// int32_t. Build a flat one-byte string whose (corrupted) length is 2^31 and
// whose characters encode to exactly 0xFFFFFFFF UTF-8 bytes: one ASCII 'e'
// followed by 2^31 - 1 bytes of 0xff (2 UTF-8 bytes each).

if (typeof Intl !== 'undefined') {
  const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
  const u8 = new Uint8Array(memory.buffer);

  const kHeapObjectTag = 1;
  const kElementsOffset = 8;
  const kConsStringFirstOffset = 12;
  const kStringLengthOffset = 8;
  const kSeqOneByteCharsOffset = 12;
  const kFixedArrayHeaderSize = 8;

  const kChunkSize = 536 * 1000 * 1000;
  const kCorruptedLength = 2 ** 31;

  const keepAlive = [];
  function allocFlatOneByteChunk(size) {
    const s = '\xff'.repeat(size);
    // Flatten the ConsString into a SeqOneByteString in large object space.
    s.indexOf('a');
    keepAlive.push(s);
    return memory.getUint32(
               Sandbox.getAddressOf(s) + kConsStringFirstOffset, true) -
        kHeapObjectTag;
  }

  // Allocate > 2GB of 0xff bytes in (typically) contiguous large pages.
  const chunks = [
    allocFlatOneByteChunk(kChunkSize), allocFlatOneByteChunk(kChunkSize),
    allocFlatOneByteChunk(kChunkSize), allocFlatOneByteChunk(kChunkSize),
    allocFlatOneByteChunk(100 * 1000 * 1000)
  ];
  const f1 = chunks[0];

  // Fill the small gaps between consecutive chunks with 0xff so that there are
  // no NUL bytes in [f1 + kSeqOneByteCharsOffset, +2^31).
  for (let i = 0; i < chunks.length - 1; i++) {
    const gapStart = chunks[i] + kSeqOneByteCharsOffset + kChunkSize;
    const gapEnd = chunks[i + 1] + kSeqOneByteCharsOffset;
    if (gapStart <= gapEnd && gapEnd - gapStart < 0x100000) {
      u8.fill(0xff, gapStart, gapEnd);
    }
  }
  u8[f1 + kSeqOneByteCharsOffset] = 0x65;  // 'e'

  // Expose the flat string to JS and corrupt its length.
  const arr = ['x'];
  const elements =
      memory.getUint32(Sandbox.getAddressOf(arr) + kElementsOffset, true) -
      kHeapObjectTag;
  memory.setUint32(elements + kFixedArrayHeaderSize, f1 + kHeapObjectTag, true);
  memory.setUint32(f1 + kStringLengthOffset, kCorruptedLength, true);

  new Intl.Locale(arr[0]);
  assertUnreachable('Corrupted String length should be caught by SBXCHECK');
}
