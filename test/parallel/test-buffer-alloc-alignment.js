// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('assert');
const { Buffer, constants } = require('buffer');
const { internalBinding } = require('internal/test/binding');
const { arrayBufferAlignedOffset } = internalBinding('buffer');

// Buffer.allocUnsafe(size, alignment) and Buffer.allocUnsafeSlow(size,
// alignment) return a buffer whose memory starts at an address that is a
// multiple of `alignment`.

// Addresses are not observable from JS, so alignment cannot be checked against
// `byteOffset` alone: the padding an allocation needs depends on where its
// backing store happens to land. `arrayBufferAlignedOffset(ab, alignment)`
// returns an offset into `ab` that is known to be aligned, so any other offset
// is aligned exactly when it is congruent to that one.
function assertAligned(buf, alignment) {
  const aligned = arrayBufferAlignedOffset(buf.buffer, alignment);
  // `aligned` is in [0, alignment), so adding `alignment` keeps this positive.
  const skew = (buf.byteOffset - aligned + alignment) % alignment;
  assert.strictEqual(skew, 0,
                     `byteOffset ${buf.byteOffset} is not ${alignment} byte ` +
                     `aligned (aligned offsets are ${aligned} mod ${alignment})`);
}

const alignments = [1, 2, 4, 8, 16, 64, 512, 4096, 65536];
const sizes = [0, 1, 7, 64, 65, 512, 4096, 100000];

for (const alloc of [Buffer.allocUnsafe, Buffer.allocUnsafeSlow]) {
  for (const alignment of alignments) {
    for (const size of sizes) {
      const buf = alloc(size, alignment);
      assert.strictEqual(buf.length, size);
      if (size > 0) {
        assertAligned(buf, alignment);
      }
      // The view must fit inside the (over-allocated) ArrayBuffer.
      assert.ok(buf.byteOffset + size <= buf.buffer.byteLength);
      // The whole buffer must be writable through the aligned view.
      buf.fill(0x61);
      if (size > 0) {
        assert.strictEqual(buf[0], 0x61);
        assert.strictEqual(buf[size - 1], 0x61);
      }
    }
  }

  // A zero length buffer is returned for size 0, whatever the alignment.
  assert.strictEqual(alloc(0, 4096).length, 0);

  // An aligned buffer is a normal Buffer.
  {
    const buf = alloc(32, 4096);
    assert.ok(Buffer.isBuffer(buf));
    buf.write('hello');
    assert.strictEqual(buf.toString('latin1', 0, 5), 'hello');
    assert.strictEqual(buf.subarray(1, 3).length, 2);
  }

  // Consecutive allocations do not overlap.
  {
    const a = alloc(64, 64).fill(0x01);
    const b = alloc(64, 64).fill(0x02);
    assert.strictEqual(a[0], 0x01);
    assert.strictEqual(a[63], 0x01);
    assert.strictEqual(b[0], 0x02);
    assert.strictEqual(b[63], 0x02);
  }

  // Invalid alignments.
  for (const alignment of [0, -1, -4096, 3, 5, 100, 1000, 2 ** 30 + 1]) {
    assert.throws(() => alloc(10, alignment), {
      name: /^(RangeError|TypeError)$/,
    });
  }

  for (const alignment of [1.5, NaN, Infinity]) {
    assert.throws(() => alloc(10, alignment), { code: 'ERR_OUT_OF_RANGE' });
  }

  for (const alignment of [null, '64', 64n, {}, [], true]) {
    assert.throws(() => alloc(10, alignment), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  }

  // Alignments that are not a power of two report the reason.
  assert.throws(() => alloc(10, 3), {
    code: 'ERR_INVALID_ARG_VALUE',
    message: /must be a power of two/,
  });

  // `size` plus the padding must still fit within the maximum buffer length.
  assert.throws(() => alloc(constants.MAX_LENGTH, 4096), {
    code: 'ERR_OUT_OF_RANGE',
  });

  // `size` itself is validated before `alignment`.
  assert.throws(() => alloc(-1, 64), { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => alloc(constants.MAX_LENGTH + 1, 64), {
    code: 'ERR_OUT_OF_RANGE',
  });
}

// Omitting the alignment keeps the previous behaviour.
{
  const buf = Buffer.allocUnsafeSlow(100);
  assert.strictEqual(buf.length, 100);
  assert.strictEqual(buf.byteOffset, 0);
  assert.strictEqual(buf.buffer.byteLength, 100);
}

// Buffer.allocUnsafeSlow() is never pooled, even when aligned.
{
  const a = Buffer.allocUnsafeSlow(64, 64);
  const b = Buffer.allocUnsafeSlow(64, 64);
  assert.notStrictEqual(a.buffer, b.buffer);
}

// Buffer.allocUnsafe() serves alignments up to the pool alignment from the pool,
// and allocates on its own beyond that.
{
  const bufs = [];
  for (let i = 0; i < 8; i++) {
    bufs.push(Buffer.allocUnsafe(64, 64));
  }
  // Pooled, so consecutive allocations share an ArrayBuffer. A pool may be
  // exhausted in between, hence checking that any two neighbours share one.
  assert.ok(bufs.some((buf, i) => i > 0 && buf.buffer === bufs[i - 1].buffer));
  for (const buf of bufs) {
    assertAligned(buf, 64);
  }

  // Stricter than the pool alignment, so this gets its own ArrayBuffer.
  const own = Buffer.allocUnsafe(64, 128);
  assert.ok(bufs.every((buf) => buf.buffer !== own.buffer));
}

// Aligned pooled allocations do not disturb unaligned ones. Interleave the two
// and make sure every buffer keeps its own contents.
{
  const bufs = [];
  for (let i = 0; i < 256; i++) {
    const buf = i % 2 === 0 ?
      Buffer.allocUnsafe(24) :
      Buffer.allocUnsafe(24, 16);
    if (i % 2 === 1) {
      assertAligned(buf, 16);
    }
    buf.fill(i % 256);
    bufs.push(buf);
  }
  for (let i = 0; i < bufs.length; i++) {
    assert.strictEqual(bufs[i][0], i % 256);
    assert.strictEqual(bufs[i][23], i % 256);
  }
}
